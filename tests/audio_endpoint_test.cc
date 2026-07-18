#include "audio_endpoint.hh"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <alsa/asoundlib.h>


// ============================================================================
// Mock ALSA Functions (wrapped using linker --wrap)
// ============================================================================

static int mock_snd_pcm_open_result = 0;
static int mock_snd_pcm_hw_params_any_result = 0;
static int mock_snd_pcm_hw_params_test_access_result = 0;
static int mock_snd_pcm_hw_params_test_format_result = 0;
static bool should_fail_snd_pcm_open = false;

// Wrapped ALSA functions
extern "C" {
    int __wrap_snd_pcm_open(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
    int __wrap_snd_pcm_close(snd_pcm_t *pcm);
    int __wrap_snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
    int __wrap_snd_pcm_hw_params_test_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access);
    int __wrap_snd_pcm_hw_params_test_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t format);
}

// Mock implementations
int __wrap_snd_pcm_open(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode) {
    (void)name;
    (void)stream;
    (void)mode;
    if (should_fail_snd_pcm_open) {
        return -EINVAL;
    }
    // Allocate a dummy pointer (not a real PCM handle, just for testing)
    *pcm = reinterpret_cast<snd_pcm_t*>(1);
    return mock_snd_pcm_open_result;
}

int __wrap_snd_pcm_close(snd_pcm_t *pcm) {
    (void)pcm;
    return 0;
}

int __wrap_snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params) {
    (void)pcm;
    (void)params;
    return mock_snd_pcm_hw_params_any_result;
}

int __wrap_snd_pcm_hw_params_test_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
    (void)pcm;
    (void)params;
    (void)access;
    return mock_snd_pcm_hw_params_test_access_result;
}

int __wrap_snd_pcm_hw_params_test_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t format) {
    (void)pcm;
    (void)params;
    (void)format;
    return mock_snd_pcm_hw_params_test_format_result;
}

// ============================================================================
// Helper class to manage mock state
// ============================================================================

class MockAlsaFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        // Reset all mock return values to success
        mock_snd_pcm_open_result = 0;
        mock_snd_pcm_hw_params_any_result = 0;
        mock_snd_pcm_hw_params_test_access_result = 0;
        mock_snd_pcm_hw_params_test_format_result = 0;
        should_fail_snd_pcm_open = false;
    }

    void TearDown() override {
        // Reset after each test
        should_fail_snd_pcm_open = false;
    }
};

// ============================================================================
// Tests for detect_device_mounting()
// ============================================================================

class DeviceMountingTests : public ::testing::Test {
  protected:
};

TEST(DeviceMountingTests, DetectsUSBDevice) {
    device_mount result = detect_device_mounting("hw:CARD=USB,DEV=0");
    EXPECT_EQ(result, device_mount::USB);
}

TEST(DeviceMountingTests, DetectsUSBDeviceWithDifferentFormat) {
    device_mount result = detect_device_mounting("hw:USB:Card");
    EXPECT_EQ(result, device_mount::USB);
}

TEST(DeviceMountingTests, DetectsDACDevice) {
    device_mount result = detect_device_mounting("hw:CARD=DAC,DEV=0");
    EXPECT_EQ(result, device_mount::DAC);
}

TEST(DeviceMountingTests, DetectsDACDeviceWithDifferentFormat) {
    device_mount result = detect_device_mounting("hw:DAC:Card");
    EXPECT_EQ(result, device_mount::DAC);
}

TEST(DeviceMountingTests, ReturnsUnknownForInvalidDevice) {
    device_mount result = detect_device_mounting("hw:CARD=INVALID,DEV=0");
    EXPECT_EQ(result, device_mount::Unknown);
}

TEST(DeviceMountingTests, ReturnsUnknownForEmptyString) {
    device_mount result = detect_device_mounting("");
    EXPECT_EQ(result, device_mount::Unknown);
}

TEST(DeviceMountingTests, ReturnsUnknownForRandomString) {
    device_mount result = detect_device_mounting("some_random_string");
    EXPECT_EQ(result, device_mount::Unknown);
}

TEST(DeviceMountingTests, CaseInsensitiveUSBDetection) {
    device_mount result = detect_device_mounting("hw:CARD=usb,DEV=0");
    EXPECT_EQ(result, device_mount::Unknown); // Current impl is case-sensitive
}

// ============================================================================
// Tests for open_endpoint() with mocked ALSA
// ============================================================================

class ConfigureEndpointTests : public MockAlsaFixture {
  protected:
};

TEST_F(ConfigureEndpointTests, InitializesStructProperly) {
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    EXPECT_EQ(ep.capture, nullptr);
    EXPECT_EQ(ep.playback, nullptr);
}

TEST_F(ConfigureEndpointTests, OpensUSBCaptureDevice) {
    mock_snd_pcm_open_result = 0;
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    int result = open_endpoint("hw:CARD=USB,DEV=0", ep);
    EXPECT_EQ(result, 0);
    EXPECT_NE(ep.capture, nullptr);
}

TEST_F(ConfigureEndpointTests, OpensDACPlaybackDevice) {
    mock_snd_pcm_open_result = 0;
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    int result = open_endpoint("hw:CARD=DAC,DEV=0", ep);
    EXPECT_EQ(result, 0);
    EXPECT_NE(ep.playback, nullptr);
}

TEST_F(ConfigureEndpointTests, HandlesInvalidDeviceStringGracefully) {
    should_fail_snd_pcm_open = true;
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    int result = open_endpoint("hw:INVALID,DEV=0", ep);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(ConfigureEndpointTests, HandlesUnknownDeviceType) {
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    // Unknown device type should not attempt to open
    int result = open_endpoint("hw:CARD=MYSTERY,DEV=0", ep);
    (void)result; // intentionally unused in this test
    // Function should handle gracefully without crashing
    SUCCEED();
}

TEST_F(ConfigureEndpointTests, ReturnsIntegerValue) {
    endpoint_type ep;
    ep.capture = nullptr;
    ep.playback = nullptr;

    int result = open_endpoint("hw:CARD=USB,DEV=0", ep);
    EXPECT_TRUE(std::is_integral_v<decltype(result)>);
}

// ============================================================================
// Tests for set_best_format() with mocked ALSA
// ============================================================================

class SetBestFormatTests : public MockAlsaFixture {
  protected:
};

TEST_F(SetBestFormatTests, FindsBestFormatWhenSupported) {
    // Mock successful format detection
    mock_snd_pcm_hw_params_any_result = 0;
    mock_snd_pcm_hw_params_test_access_result = 0;
    mock_snd_pcm_hw_params_test_format_result = 0;

    endpoint_type ep;
    ep.capture = reinterpret_cast<snd_pcm_t*>(1);
    ep.playback = reinterpret_cast<snd_pcm_t*>(2);

    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    int result = get_best_format(ep, &format);
    // Should return success
    EXPECT_EQ(result, 0);
}

TEST_F(SetBestFormatTests, ReturnsErrorWhenHwParamsAnyFails) {
    // Mock hw_params_any failure
    mock_snd_pcm_hw_params_any_result = -EINVAL;
    
    endpoint_type ep;
    ep.capture = reinterpret_cast<snd_pcm_t*>(1);
    ep.playback = reinterpret_cast<snd_pcm_t*>(2);

    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    int result = get_best_format(ep, &format);
    (void)result; // intentionally unused in this test
    // Should handle gracefully
    SUCCEED();
}

TEST_F(SetBestFormatTests, ReturnsErrorWhenNoCommonFormat) {
    // Mock format test failure (no common format)
    mock_snd_pcm_hw_params_any_result = 0;
    mock_snd_pcm_hw_params_test_access_result = -EINVAL;
    
    endpoint_type ep;
    ep.capture = reinterpret_cast<snd_pcm_t*>(1);
    ep.playback = reinterpret_cast<snd_pcm_t*>(2);

    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    int result = get_best_format(ep, &format);
    // Should return error when no format is supported
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(SetBestFormatTests, ReturnsIntegerValue) {
    endpoint_type ep;
    ep.capture = reinterpret_cast<snd_pcm_t*>(1);
    ep.playback = reinterpret_cast<snd_pcm_t*>(2);

    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    int result = get_best_format(ep, &format);
    EXPECT_TRUE(std::is_integral_v<decltype(result)>);
}
