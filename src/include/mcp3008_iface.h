#ifndef MCP3008_IFACE_HH
#define MCP3008_IFACE_HH

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SPI_SPEED_HZ 1350000
#define MCP3008_CHANNELS 8

typedef struct {
    const char *spi_device;
    uint32_t speed_hz;
    uint8_t mode;
    uint8_t bits_per_word;
} mcp3008_config_t;


int open_spi_config(const mcp3008_config_t *config);


int open_spi(const char *device);

int read_mcp3008(int fd, uint8_t channel);
#ifdef __cplusplus
}
#endif
#endif // MCP3008_IFACE_HH
