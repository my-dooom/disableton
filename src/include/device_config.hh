#pragma once

#include <alsa/asoundlib.h>

#define SAMPLE_RATE 48000 // audio sample rate in hz
#define CHANNELS 2        // stereo
#define PERIOD_FRAMES 512 // target period size in frames
#define BUFFER_PERIODS 4  // total ring buffer = PERIOD_FRAMES * BUFFER_PERIODS

struct device_role {
    snd_pcm_t *handle;
    const char *name;
};

// Configures one ALSA PCM endpoint for mmap interleaved streaming.
// Returns 0 on success, or a negative ALSA error code on failure.
int configure_device(snd_pcm_t *handle, const char *role, snd_pcm_format_t stream_format,
                     unsigned sample_rate, unsigned channels, snd_pcm_uframes_t period_frames,
                     unsigned buffer_periods);
