#include "mcp3008_iface.h"
#include <stdint.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

int open_spi(const char *device) {
    mcp3008_config_t config;
    config.spi_device = device;
    config.speed_hz = SPI_SPEED_HZ;
    config.mode = SPI_MODE_0;
    config.bits_per_word = 8;
    return open_spi_config(&config);
}

/**
 * Opens and configures an SPI device using the supplied MCP3008 settings.
 *
 * @param config SPI device path, mode, word size, and clock speed to apply.
 * @return An open file descriptor owned by the caller, or -1 on failure.
 */
int open_spi_config(const mcp3008_config_t *config) {
    if (!config || !config->spi_device)
        return -1;

    int fd = open(config->spi_device, O_RDWR);
    if (fd < 0)
        return -1;

    uint8_t mode = config->mode;
    uint8_t bits = config->bits_per_word;
    uint32_t speed = config->speed_hz;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        close(fd);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        close(fd);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int read_mcp3008(int fd, uint8_t channel) {
    if (fd < 0 || channel >= MCP3008_CHANNELS)
        return -1;
    // Build the command to read from the specified channel
    uint8_t tx[3] = {
        0x01,                  // Start bit
        (uint8_t)((0x08 | (channel & 0x07)) << 4), // Single-ended mode, channel selection
        0x00                   // dummy byte to clock out the data
    };
    uint8_t rx[3] = {0};

    // zero out the transfer structure
    struct spi_ioc_transfer tr = {tr.tx_buf = (unsigned long)tx,
                                  tr.rx_buf = (unsigned long)rx,
                                  tr.len = 3,
                                  tr.speed_hz = SPI_SPEED_HZ,
                                  tr.delay_usecs = 0,
                                  tr.bits_per_word = 8,
                                  tr.cs_change = 0,
                                  tr.tx_nbits = 0,
                                  tr.rx_nbits = 0,
                                  tr.word_delay_usecs = 0,
                                  tr.pad = 0};
    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        return -1;
    }

    // Combine the received bytes into a single integer value
    ret = ((rx[1] & 0x03) << 8) | rx[2];
    if (ret < 0 || ret > 1023) {
        return -1; // Value out of range for MCP3008
    }
    return ret;
}

