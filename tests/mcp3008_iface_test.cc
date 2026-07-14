#include "mcp3008_iface.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <cstring>

// ============================================================================
// Mock POSIX/SPI Functions (wrapped using linker --wrap)
// ============================================================================

static bool mock_open_should_fail = false;
static int mock_open_fd = 42;
static const char *last_open_pathname = nullptr;

static int mock_ioctl_wr_mode_result = 0;
static int mock_ioctl_wr_bits_result = 0;
static int mock_ioctl_wr_speed_result = 0;
static int mock_ioctl_message_result = 3; // number of bytes transferred, matches tr.len
static uint8_t mock_rx_bytes[3] = {0, 0, 0};
static uint8_t last_tx_bytes[3] = {0, 0, 0};

extern "C" {
    int __wrap_open(const char *pathname, int flags);
    int __wrap_close(int fd);
    int __wrap_ioctl(int fd, unsigned long request, void *argp);
}

int __wrap_open(const char *pathname, int flags) {
    (void)flags;
    last_open_pathname = pathname;
    if (mock_open_should_fail) {
        return -1;
    }
    return mock_open_fd;
}

int __wrap_close(int fd) {
    (void)fd;
    return 0;
}

int __wrap_ioctl(int fd, unsigned long request, void *argp) {
    (void)fd;
    if (request == SPI_IOC_WR_MODE) {
        return mock_ioctl_wr_mode_result;
    }
    if (request == SPI_IOC_WR_BITS_PER_WORD) {
        return mock_ioctl_wr_bits_result;
    }
    if (request == SPI_IOC_WR_MAX_SPEED_HZ) {
        return mock_ioctl_wr_speed_result;
    }
    if (request == SPI_IOC_MESSAGE(1)) {
        // SPI_IOC_MESSAGE(1) submits one full-duplex SPI transfer: the driver sends
        // the MCP3008 channel-selection command through tx_buf and receives the ADC
        // conversion bytes through rx_buf. Record the transmitted command so tests
        // can assert it, then populate the receive buffer only for a successful
        // mocked transfer.
        auto *tr = reinterpret_cast<struct spi_ioc_transfer *>(argp);
        auto *tx = reinterpret_cast<uint8_t *>(tr->tx_buf);
        auto *rx = reinterpret_cast<uint8_t *>(tr->rx_buf);
        std::memcpy(last_tx_bytes, tx, sizeof(last_tx_bytes));
        if (mock_ioctl_message_result >= 1) {
            std::memcpy(rx, mock_rx_bytes, sizeof(mock_rx_bytes));
        }
        return mock_ioctl_message_result;
    }
    return 0;
}

// ============================================================================
// Fixture to reset mock state between tests
// ============================================================================

class Mcp3008Fixture : public ::testing::Test {
  protected:
    void SetUp() override {
        mock_open_should_fail = false;
        mock_open_fd = 42;
        last_open_pathname = nullptr;

        mock_ioctl_wr_mode_result = 0;
        mock_ioctl_wr_bits_result = 0;
        mock_ioctl_wr_speed_result = 0;
        mock_ioctl_message_result = 3;
        std::memset(mock_rx_bytes, 0, sizeof(mock_rx_bytes));
        std::memset(last_tx_bytes, 0, sizeof(last_tx_bytes));
    }
};

// ============================================================================
// Tests for open_spi_config()
// ============================================================================

class OpenSpiConfigTests : public Mcp3008Fixture {};

TEST_F(OpenSpiConfigTests, ReturnsErrorForNullConfig) {
    EXPECT_EQ(open_spi_config(nullptr), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsErrorForNullDevicePath) {
    mcp3008_config_t config{nullptr, SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsErrorWhenOpenFails) {
    mock_open_should_fail = true;
    mcp3008_config_t config{"/dev/spidev0.0", SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsErrorWhenSetModeFails) {
    mock_ioctl_wr_mode_result = -1;
    mcp3008_config_t config{"/dev/spidev0.0", SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsErrorWhenSetBitsPerWordFails) {
    mock_ioctl_wr_bits_result = -1;
    mcp3008_config_t config{"/dev/spidev0.0", SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsErrorWhenSetSpeedFails) {
    mock_ioctl_wr_speed_result = -1;
    mcp3008_config_t config{"/dev/spidev0.0", SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), -1);
}

TEST_F(OpenSpiConfigTests, ReturnsFileDescriptorOnSuccess) {
    mcp3008_config_t config{"/dev/spidev0.0", SPI_SPEED_HZ, SPI_MODE_0, 8};
    EXPECT_EQ(open_spi_config(&config), mock_open_fd);
    EXPECT_STREQ(last_open_pathname, "/dev/spidev0.0");
}

// ============================================================================
// Tests for open_spi()
// ============================================================================

class OpenSpiTests : public Mcp3008Fixture {};

TEST_F(OpenSpiTests, ReturnsFileDescriptorOnSuccess) {
    EXPECT_EQ(open_spi("/dev/spidev0.0"), mock_open_fd);
}

TEST_F(OpenSpiTests, ReturnsErrorWhenOpenFails) {
    mock_open_should_fail = true;
    EXPECT_EQ(open_spi("/dev/spidev0.0"), -1);
}

TEST_F(OpenSpiTests, ReturnsErrorWhenIoctlFails) {
    mock_ioctl_wr_mode_result = -1;
    EXPECT_EQ(open_spi("/dev/spidev0.0"), -1);
}

// ============================================================================
// Tests for read_mcp3008()
// ============================================================================

class ReadMcp3008Tests : public Mcp3008Fixture {};

TEST_F(ReadMcp3008Tests, ReturnsErrorForNegativeFd) {
    EXPECT_EQ(read_mcp3008(-1, 0), -1);
}

TEST_F(ReadMcp3008Tests, ReturnsErrorForChannelOutOfRange) {
    EXPECT_EQ(read_mcp3008(mock_open_fd, MCP3008_CHANNELS), -1);
}

TEST_F(ReadMcp3008Tests, ReturnsErrorWhenIoctlTransferFails) {
    mock_ioctl_message_result = -1;
    EXPECT_EQ(read_mcp3008(mock_open_fd, 0), -1);
}

TEST_F(ReadMcp3008Tests, DecodesReturnedValueFromRxBytes) {
    // rx[1] & 0x03 == 0x02, rx[2] == 0x00 -> (2 << 8) | 0 == 512
    mock_rx_bytes[1] = 0x02;
    mock_rx_bytes[2] = 0x00;
    EXPECT_EQ(read_mcp3008(mock_open_fd, 0), 512);
}

TEST_F(ReadMcp3008Tests, DecodesMaxValueFromRxBytes) {
    // rx[1] & 0x03 == 0x03, rx[2] == 0xFF -> (3 << 8) | 255 == 1023
    mock_rx_bytes[1] = 0x03;
    mock_rx_bytes[2] = 0xFF;
    EXPECT_EQ(read_mcp3008(mock_open_fd, 0), 1023);
}

TEST_F(ReadMcp3008Tests, BuildsCorrectCommandBytesForChannel) {
    read_mcp3008(mock_open_fd, 3);
    EXPECT_EQ(last_tx_bytes[0], 0x01);
    EXPECT_EQ(last_tx_bytes[1], (uint8_t)((0x08 | (3 & 0x07)) << 4));
    EXPECT_EQ(last_tx_bytes[2], 0x00);
}
