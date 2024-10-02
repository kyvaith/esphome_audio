/*
    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include "spdif.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace spdif_audio {

// Allow the i2s_audio component to use the first port
#define I2S_NUM static_cast<i2s_port_t>(I2S_NUM_MAX - 1)

#define I2S_BITS_PER_SAMPLE static_cast<i2s_bits_per_sample_t>(32)  // Stereo 16-bit PCM
#define BMC_BITS_PER_SAMPLE 64                                      // Stereo 32-bit BMC
#define SPDIF_BLOCK_SAMPLES 192
#define SPDIF_BLOCK_SIZE (SPDIF_BLOCK_SAMPLES * (BMC_BITS_PER_SAMPLE / 8))
#define SPDIF_BUF_ARRAY_SIZE (SPDIF_BLOCK_SIZE / sizeof(uint32_t))  // One block, 1536 bytes
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN (SPDIF_BLOCK_SAMPLES * BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE)  // One block

static uint32_t spdif_buf[SPDIF_BUF_ARRAY_SIZE];
static uint32_t *spdif_ptr;

/*
 * 8bit PCM to 16bit BMC conversion table, LSb first, 1 end
 */
static const uint16_t bmc_tab[256] = {
    0x3333, 0xb333, 0xd333, 0x5333, 0xcb33, 0x4b33, 0x2b33, 0xab33, 0xcd33, 0x4d33, 0x2d33, 0xad33, 0x3533, 0xb533,
    0xd533, 0x5533, 0xccb3, 0x4cb3, 0x2cb3, 0xacb3, 0x34b3, 0xb4b3, 0xd4b3, 0x54b3, 0x32b3, 0xb2b3, 0xd2b3, 0x52b3,
    0xcab3, 0x4ab3, 0x2ab3, 0xaab3, 0xccd3, 0x4cd3, 0x2cd3, 0xacd3, 0x34d3, 0xb4d3, 0xd4d3, 0x54d3, 0x32d3, 0xb2d3,
    0xd2d3, 0x52d3, 0xcad3, 0x4ad3, 0x2ad3, 0xaad3, 0x3353, 0xb353, 0xd353, 0x5353, 0xcb53, 0x4b53, 0x2b53, 0xab53,
    0xcd53, 0x4d53, 0x2d53, 0xad53, 0x3553, 0xb553, 0xd553, 0x5553, 0xcccb, 0x4ccb, 0x2ccb, 0xaccb, 0x34cb, 0xb4cb,
    0xd4cb, 0x54cb, 0x32cb, 0xb2cb, 0xd2cb, 0x52cb, 0xcacb, 0x4acb, 0x2acb, 0xaacb, 0x334b, 0xb34b, 0xd34b, 0x534b,
    0xcb4b, 0x4b4b, 0x2b4b, 0xab4b, 0xcd4b, 0x4d4b, 0x2d4b, 0xad4b, 0x354b, 0xb54b, 0xd54b, 0x554b, 0x332b, 0xb32b,
    0xd32b, 0x532b, 0xcb2b, 0x4b2b, 0x2b2b, 0xab2b, 0xcd2b, 0x4d2b, 0x2d2b, 0xad2b, 0x352b, 0xb52b, 0xd52b, 0x552b,
    0xccab, 0x4cab, 0x2cab, 0xacab, 0x34ab, 0xb4ab, 0xd4ab, 0x54ab, 0x32ab, 0xb2ab, 0xd2ab, 0x52ab, 0xcaab, 0x4aab,
    0x2aab, 0xaaab, 0xcccd, 0x4ccd, 0x2ccd, 0xaccd, 0x34cd, 0xb4cd, 0xd4cd, 0x54cd, 0x32cd, 0xb2cd, 0xd2cd, 0x52cd,
    0xcacd, 0x4acd, 0x2acd, 0xaacd, 0x334d, 0xb34d, 0xd34d, 0x534d, 0xcb4d, 0x4b4d, 0x2b4d, 0xab4d, 0xcd4d, 0x4d4d,
    0x2d4d, 0xad4d, 0x354d, 0xb54d, 0xd54d, 0x554d, 0x332d, 0xb32d, 0xd32d, 0x532d, 0xcb2d, 0x4b2d, 0x2b2d, 0xab2d,
    0xcd2d, 0x4d2d, 0x2d2d, 0xad2d, 0x352d, 0xb52d, 0xd52d, 0x552d, 0xccad, 0x4cad, 0x2cad, 0xacad, 0x34ad, 0xb4ad,
    0xd4ad, 0x54ad, 0x32ad, 0xb2ad, 0xd2ad, 0x52ad, 0xcaad, 0x4aad, 0x2aad, 0xaaad, 0x3335, 0xb335, 0xd335, 0x5335,
    0xcb35, 0x4b35, 0x2b35, 0xab35, 0xcd35, 0x4d35, 0x2d35, 0xad35, 0x3535, 0xb535, 0xd535, 0x5535, 0xccb5, 0x4cb5,
    0x2cb5, 0xacb5, 0x34b5, 0xb4b5, 0xd4b5, 0x54b5, 0x32b5, 0xb2b5, 0xd2b5, 0x52b5, 0xcab5, 0x4ab5, 0x2ab5, 0xaab5,
    0xccd5, 0x4cd5, 0x2cd5, 0xacd5, 0x34d5, 0xb4d5, 0xd4d5, 0x54d5, 0x32d5, 0xb2d5, 0xd2d5, 0x52d5, 0xcad5, 0x4ad5,
    0x2ad5, 0xaad5, 0x3355, 0xb355, 0xd355, 0x5355, 0xcb55, 0x4b55, 0x2b55, 0xab55, 0xcd55, 0x4d55, 0x2d55, 0xad55,
    0x3555, 0xb555, 0xd555, 0x5555,
};

// BMC preamble
#define BMC_B 0x33173333  // block start
#define BMC_M 0x331d3333  // left ch
#define BMC_W 0x331b3333  // right ch
#define BMC_MW_DIF (BMC_M ^ BMC_W)
#define SYNC_OFFSET 2  // byte offset of SYNC
#define SYNC_FLIP ((BMC_B ^ BMC_M) >> (SYNC_OFFSET * 8))

#if SPDIF_FILL_SILENCE
int16_t silence[SPDIF_BLOCK_SAMPLES * BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE];
#endif

QueueHandle_t i2s_event_queue;

static const char *const TAG = "spdif";

// initialize S/PDIF buffer
static void spdif_buf_init(void) {
  int i;
  uint32_t bmc_mw = BMC_W;

  for (i = 0; i < SPDIF_BUF_ARRAY_SIZE; i += 2) {
    spdif_buf[i] = bmc_mw ^= BMC_MW_DIF;
  }
  esph_log_i(TAG, "SPDIF buffer initialized to %zu bytes", sizeof(spdif_buf));

#if SPDIF_FILL_SILENCE
  memset(silence, 0, sizeof(silence));
#endif
}

void i2s_event_task(void *arg) {
  i2s_event_t i2s_event;
  int64_t last_error_log_time = 0;
  int64_t last_overflow_log_time = 0;
  // 1 second in microseconds
  const int64_t min_log_interval_us = 1000000;

  while (1) {
    if (xQueueReceive(i2s_event_queue, &i2s_event, portMAX_DELAY)) {
      int64_t current_time = esp_timer_get_time();

      if (i2s_event.type == I2S_EVENT_DMA_ERROR) {
#if SPDIF_DEBUG
        if (current_time - last_error_log_time >= min_log_interval_us) {
          esph_log_e(TAG, "I2S_EVENT_DMA_ERROR");
          last_error_log_time = current_time;
        }
#endif
      } else if (i2s_event.type == I2S_EVENT_TX_Q_OVF) {
        // I2S DMA sending queue overflowed, the oldest data has been overwritten
        // by the new data in the DMA buffer
#if SPDIF_DEBUG
        if (current_time - last_overflow_log_time >= min_log_interval_us) {
          esph_log_e(TAG, "I2S_EVENT_TX_Q_OVF");
          last_overflow_log_time = current_time;
        }
#endif
#if SPDIF_FILL_SILENCE
        // Queue DMA a couple buffers full of silence when we don't have anything else to play
        spdif_ptr = spdif_buf;
        spdif_write(silence, sizeof(silence), 0);
        spdif_write(silence, sizeof(silence), 0);
#endif
      }
    }
  }
}

// initialize I2S for S/PDIF transmission
void spdif_init(uint32_t rate) {
  uint32_t sample_rate = rate * BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE;
  i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = true,
#if SPDIF_FILL_SILENCE
    .tx_desc_auto_clear = false,
#else
    .tx_desc_auto_clear = true,
#endif
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
    .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
#if SOC_I2S_SUPPORTS_TDM
    .chan_mask = I2S_CHANNEL_MONO,
    .total_chan = 0,
    .left_align = false,
    .big_edin = false,
    .bit_order_msb = false,
    .skip_msk = false,
#endif
  };
  i2s_pin_config_t pin_config = {
      .mck_io_num = -1,
      .bck_io_num = -1,
      .ws_io_num = -1,
      .data_out_num = SPDIF_DATA_PIN,
      .data_in_num = -1,
  };

#if SPDIF_DEBUG || SPDIF_FILL_SILENCE
  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 10, &i2s_event_queue));
  xTaskCreate(i2s_event_task, "i2s_event_task", 3072, NULL, 10, NULL);
#else
  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL));
#endif

  ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));

  // initialize S/PDIF buffer
  spdif_buf_init();
  spdif_ptr = spdif_buf;
}

void spdif_deinit() { i2s_driver_uninstall(I2S_NUM); }

// write audio data to I2S buffer
esp_err_t spdif_write(const void *src, size_t size, TickType_t ticks_to_wait) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(src);

  while (p < (uint8_t *) src + size) {
    // convert PCM 16bit data to BMC 32bit pulse pattern
    // We cast to int16_t to avoid sign extension issues when XOR-ing
    *(spdif_ptr + 1) =
        (uint32_t) (((static_cast<int16_t>(bmc_tab[*p]) << 16) ^ static_cast<int16_t>(bmc_tab[*(p + 1)])) << 1) >> 1;

    p += 2;
    spdif_ptr += 2;  // advance to next audio data

    if (spdif_ptr >= &spdif_buf[SPDIF_BUF_ARRAY_SIZE]) {
      size_t i2s_write_len;

      // set block start preamble
      ((uint8_t *) spdif_buf)[SYNC_OFFSET] ^= SYNC_FLIP;

      esp_err_t err = i2s_write(I2S_NUM, spdif_buf, sizeof(spdif_buf), &i2s_write_len, ticks_to_wait);
      if (err != ESP_OK) {
        return err;
      }

      spdif_ptr = spdif_buf;
    }
  }

#if SPDIF_DEBUG
  static uint64_t total_bytes = 0;
  static uint64_t last_log_time = 0;
  static uint64_t last_log_bytes = 0;

  total_bytes += size;
  int64_t current_time = esp_timer_get_time();

  if (last_log_time == 0) {
    last_log_time = current_time;
    last_log_bytes = total_bytes;
  }

  // Check if it's time to log sample statistics (every minute)
  if (current_time - last_log_time >= 60000000) {
    uint64_t elapsed_time = current_time - last_log_time;
    uint64_t bytes_since_last_log = total_bytes - last_log_bytes;
    // 4 bytes per 16-bit stereo sample
    uint64_t samples = bytes_since_last_log / 4;
    float seconds = elapsed_time / 1000000.0f;
    float hz = samples / seconds;

    esph_log_d(TAG, "%llu samples in %.2fs (%.2fHz)", samples, seconds, hz);

    // Reset for next log
    last_log_time = current_time;
    last_log_bytes = total_bytes;
  }
#endif

  return ESP_OK;
}

// change S/PDIF sample rate
void spdif_set_sample_rates(uint32_t rate) {
  // uninstall and reinstall I2S driver for avoiding I2S bug
  spdif_deinit();
  spdif_init(rate);
}

}  // namespace spdif_audio
}  // namespace esphome