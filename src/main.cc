#include "audio_endpoint.hh"

int main() {
    configure_endpoint(const std::string &endpoint, endpoint_type &direction,
                       snd_pcm_format_t &audio_format);

    return 0;
}
