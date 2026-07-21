#pragma once

#include <alsa/asoundlib.h>

#include "processing_dma.hh"

// processes one capture->playback block and advances loop_count on success.
inline bool process_block(snd_pcm_t *capture_handle, snd_pcm_t *playback_handle,
                          snd_pcm_uframes_t frames) {
    // Route all runtime block handling through the mmap implementation module.
    return process_block_mmap_impl(capture_handle, playback_handle, frames);
}
