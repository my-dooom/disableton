#include "processing_dma.hh"
#include <algorithm>
#include <vector>

namespace {
inline void release_windows(snd_pcm_t *capture_handle, snd_pcm_uframes_t cap_offset,
                             snd_pcm_t *playback_handle, snd_pcm_uframes_t play_offset) {
    snd_pcm_mmap_commit(capture_handle, cap_offset, 0);
    snd_pcm_mmap_commit(playback_handle, play_offset, 0);
}
} // namespace

bool process_block_mmap_impl(snd_pcm_t *capture_handle, snd_pcm_t *playback_handle,
                              snd_pcm_uframes_t frames) {
    static bool playback_started = false;
    static bool priming_done = false;
    static std::vector<char> stash;              // mic bucket, holds sound before speaker wake
    static snd_pcm_uframes_t stashed_frames = 0;  // how full bucket is right now
    static snd_pcm_uframes_t stash_target = 0;    // how full bucket must get (whole speaker buffer)
    int err;

    // speaker already run outside? ok, both flag say true, skip fill-bucket dance
    if (!playback_started && snd_pcm_state(playback_handle) == SND_PCM_STATE_RUNNING) {
        playback_started = true;
        priming_done = true;
    }

    // mic broke? fix now
    if (snd_pcm_state(capture_handle) == SND_PCM_STATE_XRUN) {
        recover(capture_handle, -EPIPE);
    }
    // speaker broke, and we past fill-bucket phase? fix, and go fill bucket again, safest
    if (priming_done && snd_pcm_state(playback_handle) == SND_PCM_STATE_XRUN) {
        recover(playback_handle, -EPIPE);
        playback_started = false;
        priming_done = false;
        stashed_frames = 0;
    }

    // wait for mic sound. no spin like crazy hamster
    int wait_result = snd_pcm_wait(capture_handle, 20);
    if (wait_result == 0) {
        return false; // no sound yet, not broke, just slow
    }
    if (wait_result < 0) {
        fprintf(stderr, "process_block: capture wait error: %s\n", snd_strerror(wait_result));
        recover(capture_handle, wait_result);
        return false;
    }

    // poke kernel, make it tell truth about mic buffer
    snd_pcm_sframes_t cap_avail_check = snd_pcm_avail_update(capture_handle);
    if (cap_avail_check < 0) {
        fprintf(stderr, "process_block: capture avail error: %s\n",
                snd_strerror(static_cast<int>(cap_avail_check)));
        recover(capture_handle, -EPIPE);
        return false;
    }

    // same for speaker, only matter once speaker already awake
    if (priming_done && playback_started) {
        snd_pcm_sframes_t play_avail_check = snd_pcm_avail_update(playback_handle);
        if (play_avail_check < 0) {
            fprintf(stderr, "process_block: playback avail error: %s\n",
                    snd_strerror(static_cast<int>(play_avail_check)));
            recover(playback_handle, -EPIPE);
            playback_started = false;
            priming_done = false;
            stashed_frames = 0;
            return false;
        }
    }

    // --- open mic window, same for priming and steady state ---
    const snd_pcm_channel_area_t *cap_areas;
    snd_pcm_uframes_t cap_offset, cap_frames = frames;

    err = snd_pcm_mmap_begin(capture_handle, &cap_areas, &cap_offset, &cap_frames);
    if (err < 0) {
        fprintf(stderr, "capture mmap begin error: %s\n", snd_strerror(err));
        recover(capture_handle, err);
        return false;
    }

    const unsigned bytes_per_frame = cap_areas[0].step / 8;
    char *cap_ptr = area_byte_ptr(cap_areas, cap_offset);

    // ============ priming phase: fill mic bucket, no speaker touch yet ============
    if (!priming_done) {
        if (stash_target == 0) {
            // ask kernel how big speaker belly really is, match bucket to that
            snd_pcm_uframes_t buf_size = 0, period_size = 0;
            if (snd_pcm_get_params(playback_handle, &buf_size, &period_size) == 0 && buf_size > 0) {
                stash_target = buf_size;
            } else {
                stash_target = frames * 4; // kernel no answer, guess safe number
            }
            stash.resize(static_cast<size_t>(stash_target) * bytes_per_frame);
            stashed_frames = 0;
        }

        snd_pcm_uframes_t room = stash_target - stashed_frames;
        snd_pcm_uframes_t take = std::min(cap_frames, room);

        if (take == 0) {
            // bucket full but flag not flipped, just close window and bail this round
            snd_pcm_mmap_commit(capture_handle, cap_offset, 0);
            return false;
        }

        memcpy(stash.data() + static_cast<size_t>(stashed_frames) * bytes_per_frame, cap_ptr,
               static_cast<size_t>(take) * bytes_per_frame);

        snd_pcm_sframes_t committed = snd_pcm_mmap_commit(capture_handle, cap_offset, take);
        if (committed < 0 || static_cast<snd_pcm_uframes_t>(committed) != take) {
            fprintf(stderr, "capture commit error (priming): %s\n",
                    snd_strerror(static_cast<int>(committed)));
            recover(capture_handle, static_cast<int>(committed));
            return false;
        }
        stashed_frames += take;

        if (stashed_frames < stash_target) {
            return true; // bucket not full yet, come back next round
        }

        // bucket full! dump whole thing into speaker belly, maybe need few gulps
        snd_pcm_uframes_t to_flush = stashed_frames;
        size_t flushed_bytes = 0;
        while (to_flush > 0) {
            const snd_pcm_channel_area_t *play_areas;
            snd_pcm_uframes_t play_offset, play_room = to_flush;

            err = snd_pcm_mmap_begin(playback_handle, &play_areas, &play_offset, &play_room);
            if (err < 0) {
                fprintf(stderr, "playback mmap begin error (flush): %s\n", snd_strerror(err));
                recover(playback_handle, err);
                stashed_frames = 0; // start fill-bucket dance over
                return false;
            }
            if (play_room == 0) {
                snd_pcm_mmap_commit(playback_handle, play_offset, 0);
                break; // speaker no room somehow, bail safe, try whole dance again later
            }

            memcpy(area_byte_ptr(play_areas, play_offset), stash.data() + flushed_bytes,
                   static_cast<size_t>(play_room) * bytes_per_frame);

            snd_pcm_sframes_t play_committed =
                snd_pcm_mmap_commit(playback_handle, play_offset, play_room);
            if (play_committed < 0 || static_cast<snd_pcm_uframes_t>(play_committed) != play_room) {
                fprintf(stderr, "playback commit error (flush): %s\n",
                        snd_strerror(static_cast<int>(play_committed)));
                recover(playback_handle, static_cast<int>(play_committed));
                stashed_frames = 0;
                return false;
            }

            flushed_bytes += static_cast<size_t>(play_room) * bytes_per_frame;
            to_flush -= play_room;
        }

        // speaker belly full of mic sound now, wake speaker up
        err = snd_pcm_start(playback_handle);
        if (err < 0) {
            fprintf(stderr, "playback start error: %s\n", snd_strerror(err));
            recover(playback_handle, err);
            stashed_frames = 0;
            return false;
        }

        playback_started = true;
        priming_done = true;
        stashed_frames = 0; // bucket empty, done its job, not used again after this
        return true;
    }

    // ============ steady state: mic bucket already filled speaker once, now just dribble ============
    const snd_pcm_channel_area_t *play_areas;
    snd_pcm_uframes_t play_offset, play_frames = frames;

    err = snd_pcm_mmap_begin(playback_handle, &play_areas, &play_offset, &play_frames);
    if (err < 0) {
        fprintf(stderr, "playback mmap begin error: %s\n", snd_strerror(err));
        snd_pcm_mmap_commit(capture_handle, cap_offset, 0); // give back mic window, no leak
        recover(playback_handle, err);
        playback_started = false;
        priming_done = false;
        stashed_frames = 0;
        return false;
    }

    snd_pcm_uframes_t n = std::min(cap_frames, play_frames);
    if (n == 0) {
        release_windows(capture_handle, cap_offset, playback_handle, play_offset);
        return false;
    }

    memcpy(area_byte_ptr(play_areas, play_offset), cap_ptr,
           static_cast<size_t>(n) * bytes_per_frame);

    snd_pcm_sframes_t committed = snd_pcm_mmap_commit(capture_handle, cap_offset, n);
    if (committed < 0 || static_cast<snd_pcm_uframes_t>(committed) != n) {
        fprintf(stderr, "capture commit error: %s\n", snd_strerror(static_cast<int>(committed)));
        recover(capture_handle, static_cast<int>(committed));
        return false;
    }

    committed = snd_pcm_mmap_commit(playback_handle, play_offset, n);
    if (committed < 0 || static_cast<snd_pcm_uframes_t>(committed) != n) {
        fprintf(stderr, "playback commit error: %s\n", snd_strerror(static_cast<int>(committed)));
        recover(playback_handle, static_cast<int>(committed));
        playback_started = false;
        priming_done = false;
        stashed_frames = 0;
        return false;
    }

    return true; // ugga. sound move good this time
}