#include "audio_endpoint.hh"
#include <alsa/asoundlib.h>
#include <iostream>
// std::string capture_device = "hw:CARD=USB,DEV=0";

// std::string playback_device = "hw:CARD=DAC,DEV=0";


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

int open_endpoint(const std::string &endpoint, endpoint_type &direction) {

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


int get_best_format(endpoint_type &direction, snd_pcm_format_t *out_format) {
    *out_format = SND_PCM_FORMAT_UNKNOWN;
    for (snd_pcm_format_t format_candidate : format_candidates) {
        if (is_format_supported(direction.capture, format_candidate) &&
            is_format_supported(direction.playback, format_candidate)) {
            *out_format = format_candidate;
            return 0;
        }
    }

    // No shared format: report the failure but leave endpoint teardown to the
    // caller, which already owns the handles via RAII.
    std::cerr << "no common mmap-capable PCM format between capture and playback hw devices\n";
    return -EINVAL;
}
