#include <alsa/asoundlib.h>
#include <string>

enum class device_mount {
    Unknown,
    USB,
    DAC,
};

// TODO: When GUI is ready make a format selector

const snd_pcm_format_t format_candidates[] = {
    SND_PCM_FORMAT_FLOAT_LE,
    SND_PCM_FORMAT_S32_LE,
    SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_S16_LE,
};

typedef struct {
    // Handles for alsa devices
    snd_pcm_t *capture;
    snd_pcm_t *playback;
} endpoint_type;

int configure_endpoint(const std::string &endpoint, endpoint_type &direction,
                       snd_pcm_format_t &audio_format);
