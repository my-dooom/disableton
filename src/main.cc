#include "audio_endpoint.hh"
#include "mcp3008_iface.h"

int main() {
    std::string capture_device = "hw:CARD=USB,DEV=0";
    std::string playback_device = "hw:CARD=DAC,DEV=0";

    endpoint_type endpoint{nullptr, nullptr};

    if (configure_endpoint(capture_device, endpoint) != 0) {
        return 1;
    }
    if (configure_endpoint(playback_device, endpoint) != 0) {
        snd_pcm_close(endpoint.capture);
        endpoint.capture = nullptr;
        return 1;
    }
    if (get_best_format(endpoint) != 0) {
        return 1;
    }
    
    if (open_spi("/dev/spidev0.0") < 0) {
        return 1;
    }

    snd_pcm_close(endpoint.capture);
    snd_pcm_close(endpoint.playback);
    return 0;
}
