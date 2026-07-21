#include "device_config.hh"

#include <functional>
#include <iostream>
#include <string>

int configure_device(snd_pcm_t *handle, const char *role, snd_pcm_format_t stream_format,
                     unsigned sample_rate, unsigned channels, snd_pcm_uframes_t period_frames,
                     unsigned buffer_periods) {
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    unsigned int rate = sample_rate;
    unsigned int resample = 1;
    int dir = 0;
    snd_pcm_uframes_t frames = period_frames;
    snd_pcm_uframes_t buffer_frames = period_frames * buffer_periods;

    struct step_t {
        const char *name;
        std::string detail;
        std::function<int()> run;
    };

    // Order matters: ALSA requires access/format/resample to be set before rate/period/buffer
    // negotiation, and hw_params_any must come first to reset the params to the device's defaults.
    const step_t steps[] = {
        {"hw_params_any", "", [&] { return snd_pcm_hw_params_any(handle, hw_params); }},
        {"set_access", "",
         [&] {
             return snd_pcm_hw_params_set_access(handle, hw_params,
                                                 SND_PCM_ACCESS_MMAP_INTERLEAVED);
         }},
        {"set_format", snd_pcm_format_name(stream_format),
         [&] { return snd_pcm_hw_params_set_format(handle, hw_params, stream_format); }},
        {"set_rate_resample", "",
         [&] { return snd_pcm_hw_params_set_rate_resample(handle, hw_params, resample); }},
        {"set_rate_near", "",
         [&] { return snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, &dir); }},
        {"set_period_size_near", "",
         [&] { return snd_pcm_hw_params_set_period_size_near(handle, hw_params, &frames, &dir); }},
        {"set_buffer_size_near", "",
         [&] { return snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_frames); }},
        {"set_channels", "",
         [&] { return snd_pcm_hw_params_set_channels(handle, hw_params, channels); }},
        {"hw_params apply", "", [&] { return snd_pcm_hw_params(handle, hw_params); }},
    };

    for (const auto &step : steps) {
        if (const int err = step.run(); err < 0) {
            std::cerr << role << " " << step.name;
            if (!step.detail.empty()) {
                std::cerr << "(" << step.detail << ")";
            }
            std::cerr << " failed: " << snd_strerror(err) << "\n";
            return err;
        }
    }
    // std::cerr << role << " configured\n";
    return 0;
}
