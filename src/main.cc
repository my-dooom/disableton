#include "audio_endpoint.hh"
#include "device_config.hh"
#include "mcp3008_iface.h"

#include <cstdio>
#include <memory>
#include <unistd.h>

struct pcm_deleter {
    void operator()(snd_pcm_t *pcm) const {
        if (pcm != nullptr) {
            snd_pcm_close(pcm);
        }
    }
};

using pcm_handle = std::unique_ptr<snd_pcm_t, pcm_deleter>;

struct spi_handle {
    explicit spi_handle(int fd)
        : fd_(fd) {} // Takes ownership of an already-open fd; "explicit" blocks accidental int ->
                     // spi_handle conversions.
    ~spi_handle() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    spi_handle(const spi_handle &) =
        delete; // Deleted so two spi_handle objects can never both try to close the same fd.
    spi_handle &operator=(const spi_handle &) =
        delete; // Deleted for the same reason: exactly one owner at a time.

    int fd_;
};

int main() {

    endpoint_type raw_endpoint{nullptr, nullptr}; // A plain struct that only *carries* raw handles
                                                  // out of open_endpoint; it owns nothing.

    if (open_endpoint("hw:CARD=USB,DEV=0", raw_endpoint) != 0) {
        fprintf(stderr, "main: failed to open capture endpoint\n");
        return 1;
    }
    pcm_handle capture{raw_endpoint.capture};

    if (open_endpoint("hw:CARD=DAC,DEV=0", raw_endpoint) != 0) {
        fprintf(stderr, "main: failed to open playback endpoint\n");
        return 1;
    }
    pcm_handle playback{raw_endpoint.playback};

    snd_pcm_format_t stream_format = SND_PCM_FORMAT_UNKNOWN;
    endpoint_type endpoint{capture.get(), playback.get()};
    if (get_best_format(endpoint, &stream_format) != 0) {
        fprintf(stderr, "main: failed to negotiate a common stream format\n");
        return 1;
    }

    const int spi_fd = open_spi("/dev/spidev0.0");
    if (spi_fd < 0) {
        fprintf(stderr, "main: failed to open SPI device\n");
        return 1;
    }

    spi_handle spi{spi_fd};

    struct device_role {
        snd_pcm_t *handle;
        const char *name;
    };
    const device_role roles[] = {{capture.get(), "capture"}, {playback.get(), "playback"}};

    for (const auto &role : roles) {
        if (configure_device(role.handle, role.name, stream_format, SAMPLE_RATE, CHANNELS,
                              PERIOD_FRAMES, BUFFER_PERIODS) != 0) {
            fprintf(stderr, "main: failed to configure %s device\n", role.name);
            return 1;
        }
    }
    return 0;
}
