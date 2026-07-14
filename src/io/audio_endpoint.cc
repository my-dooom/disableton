#include "audio_endpoint.hh"
#include <alsa/asoundlib.h>
#include <iostream>
// std::string capture_device = "hw:CARD=USB,DEV=0";

// std::string playback_device = "hw:CARD=DAC,DEV=0";

snd_pcm_format_t g_stream_format = SND_PCM_FORMAT_FLOAT_LE;

device_mount detect_device_mounting(const std::string &endpoint) {
    if (endpoint.find("USB") != std::string::npos)
        return device_mount::USB;
    if (endpoint.find("DAC") != std::string::npos)
        return device_mount::DAC;
    return device_mount::Unknown;
}

static bool is_format_supported(snd_pcm_t *handle, snd_pcm_format_t format) {
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    if (snd_pcm_hw_params_any(handle, params) < 0) {
        return false;
    }
    return snd_pcm_hw_params_test_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED) == 0 &&
           snd_pcm_hw_params_test_format(handle, params, format) == 0;
}

int configure_endpoint(const std::string &endpoint, endpoint_type &direction) {

    int err = 0;
    device_mount mount = detect_device_mounting(endpoint);
    switch (mount) {
    case device_mount::USB:
        err = snd_pcm_open(&direction.capture, endpoint.c_str(), SND_PCM_STREAM_CAPTURE, 0);
        break;
    case device_mount::DAC:
        err = snd_pcm_open(&direction.playback, endpoint.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        break;
    case device_mount::Unknown:
        // TODO: error with logging to stderr when something is not connected
        // properly
        err = -EINVAL;
        break;
    default:
        return -EINVAL;
    }
    return err;
}

void set_best_format_internal(snd_pcm_format_t format) { g_stream_format = format; }

int get_best_format(endpoint_type &direction) {
    int err = 0;
    snd_pcm_format_t audio_format = SND_PCM_FORMAT_UNKNOWN;
    for (snd_pcm_format_t format_candidate : format_candidates) {

        if (is_format_supported(direction.capture, format_candidate) &&
            is_format_supported(direction.playback, format_candidate)) {
            audio_format = format_candidate;
            break;
        }
    }
    if (audio_format == SND_PCM_FORMAT_UNKNOWN) {
        std::cerr << "no common mmap-capable PCM format between capture and playback hw devices\n";
        snd_pcm_close(direction.capture);
        snd_pcm_close(direction.playback);
        direction.playback = nullptr;
        direction.capture = nullptr;
        return -EINVAL;
    }

    set_best_format_internal(audio_format);

    return err;
}
