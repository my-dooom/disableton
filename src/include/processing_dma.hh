#pragma once

#include <alsa/asoundlib.h>

// Recover stream state for XRUN/suspend and restart capture streams when required.
inline bool recover(snd_pcm_t *handle, int err) {
    // ALSA XRUN:
    // Capture ring buf overflowed, app read too slow, DMA overwrote unread data (overrun).
    // OR,
    // Playback ring buf underflowed, app fed too slow, DMA starved for data (underrun).
    // Recover: snd_pcm_prepare() (+ snd_pcm_start if capture stream)
    if (err == -EPIPE) {
        err = snd_pcm_prepare(handle);
        if (err < 0)
            return false;
        if (snd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE) {
            err = snd_pcm_start(handle);
            if (err < 0)
                return false;
        }
        return true;
    }
    // ALSA SUSPEND:
    // stream susbended by hw, [USB unplugged, out of power etc]
    // Recover: loop snd_pcm_resume while -EAGAIN (device still waking)
    // fall back to snd_pcm_prepare if resume unsupported or failed
    if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
        }
        if (err < 0)
            err = snd_pcm_prepare(handle);
        if (err < 0)
            return false;
        if (snd_pcm_stream(handle) == SND_PCM_STREAM_CAPTURE) {
            err = snd_pcm_start(handle);
            if (err < 0)
                return false;
        }
        return true;
    }
    return false;
}
// Convert ALSA area metadata (base/bit offset/stride) into a byte pointer.
inline char *area_byte_ptr(const snd_pcm_channel_area_t *areas, snd_pcm_uframes_t offset) {
    return static_cast<char *>(static_cast<char *>(areas[0].addr) + areas[0].first / 8 +
                               offset * areas[0].step / 8);
}

// Processes one mmap-based capture->playback block and advances loop_count on success.
bool process_block_mmap_impl(snd_pcm_t *capture_handle, snd_pcm_t *playback_handle,
                             snd_pcm_uframes_t frames);
