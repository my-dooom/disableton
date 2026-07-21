#include "processing_dma.hh"
#include <algorithm>

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
    int err;

    // playback already run outside? ok, flag say true too
    if (!playback_started && snd_pcm_state(playback_handle) == SND_PCM_STATE_RUNNING) {
        playback_started = true;
    }

    // stream broken? fix now, before more hurt
    if (snd_pcm_state(capture_handle) == SND_PCM_STATE_XRUN) {
        recover(capture_handle, -EPIPE);
    }
    if (snd_pcm_state(playback_handle) == SND_PCM_STATE_XRUN) {
        recover(playback_handle, -EPIPE);
        playback_started = false;
    }

    // wait for mic sound. no spin like crazy hamster
    int wait_result = snd_pcm_wait(capture_handle, 20);
    if (wait_result == 0) {
        fprintf(stderr, "process_block: capture wait timeout\n");
        return false; // no sound yet. not broken, just slow
    }
    if (wait_result < 0) {
        fprintf(stderr, "process_block: capture wait error: %s\n", snd_strerror(wait_result));
        recover(capture_handle, wait_result);
        return false;
    }

    // poke kernel, make it tell truth about buffer
    if (snd_pcm_avail_update(capture_handle) < 0) {
        fprintf(stderr, "process_block: capture avail error: %s\n",
                snd_strerror(snd_pcm_avail_update(capture_handle)));
        recover(capture_handle, -EPIPE);
        return false;
    }
    if (playback_started && snd_pcm_avail_update(playback_handle) < 0) {
        fprintf(stderr, "process_block: playback avail error: %s\n",
                snd_strerror(snd_pcm_avail_update(playback_handle)));
        recover(playback_handle, -EPIPE);
        playback_started = false;
        return false;
    }

    // --- open mic window ---
    const snd_pcm_channel_area_t *cap_areas;
    snd_pcm_uframes_t cap_offset, cap_frames = frames;

    err = snd_pcm_mmap_begin(capture_handle, &cap_areas, &cap_offset, &cap_frames);
    if (err < 0) {
        fprintf(stderr, "capture mmap begin error: %s\n", snd_strerror(err));
        recover(capture_handle, err);
        return false;
    }

    // --- open speaker window ---
    const snd_pcm_channel_area_t *play_areas;
    snd_pcm_uframes_t play_offset, play_frames = frames;

    err = snd_pcm_mmap_begin(playback_handle, &play_areas, &play_offset, &play_frames);
    if (err < 0) {
        fprintf(stderr, "playback mmap begin error: %s\n", snd_strerror(err));
        snd_pcm_mmap_commit(capture_handle, cap_offset, 0); // give back mic window, no use
        recover(playback_handle, err);
        playback_started = false;
        return false;
    }

    // take smaller number. both side must agree how much sound move
    snd_pcm_uframes_t n = std::min(cap_frames, play_frames);
    if (n == 0) {
        fprintf(stderr, "process_block: no frames available to move\n");
        release_windows(capture_handle, cap_offset, playback_handle, play_offset);
        return false; // nothing to move, close windows, try again later
    }

    // copy sound rock from mic hole to speaker hole
    const unsigned bytes_per_frame = cap_areas[0].step / 8;
    memcpy(area_byte_ptr(play_areas, play_offset), area_byte_ptr(cap_areas, cap_offset),
           n * bytes_per_frame);

    // tell kernel: mic gave this much, done with it
    snd_pcm_sframes_t committed = snd_pcm_mmap_commit(capture_handle, cap_offset, n);
    if (committed < 0 || static_cast<snd_pcm_uframes_t>(committed) != n) {
        fprintf(stderr, "capture commit error: %s\n", snd_strerror(static_cast<int>(committed)));
        recover(capture_handle, static_cast<int>(committed));
        return false;
    }

    // tell kernel: speaker got this much, go play
    committed = snd_pcm_mmap_commit(playback_handle, play_offset, n);
    if (committed < 0 || static_cast<snd_pcm_uframes_t>(committed) != n) {
        fprintf(stderr, "playback commit error: %s\n", snd_strerror(static_cast<int>(committed)));
        recover(playback_handle, static_cast<int>(committed));
        playback_started = false;
        return false;
    }

    // speaker got food first time. wake speaker up now
    if (!playback_started) {
        err = snd_pcm_start(playback_handle);
        if (err < 0) {
            fprintf(stderr, "playback start error: %s\n", snd_strerror(err));
            recover(playback_handle, err);
            return false;
        }
        playback_started = true;
    }

    return true; // ugga. sound move good this time
}