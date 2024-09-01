#include "spdif_pipeline_element.h"

#ifdef USE_ESP_IDF

#include "../../adf_pipeline/adf_pipeline.h"
#include "freertos/event_groups.h"

#include "../../adf_pipeline/sdk_ext.h"
#include "../spdif.h"

namespace esphome {
using namespace esp_adf;
namespace spdif_audio {

static const char *const TAG = "adf_spdif_audio";
static const uint16_t CHUNK_SIZE = 1024;

extern "C" {
esp_err_t spdif_write(const void *buffer, size_t size, TickType_t ticks_to_wait);
void spdif_init(int rate);
void spdif_set_sample_rates(int rate);
}

static esp_err_t _spdif_open(audio_element_handle_t self) {
  SPDIFStreamWriter *this_writer = (SPDIFStreamWriter *) audio_element_getdata(self);
  // esph_log_d(TAG, "_spdif_open");
  return ESP_OK;
}

static esp_err_t _spdif_close(audio_element_handle_t self) {
  // esph_log_d(TAG, "_spdif_close");
  return ESP_OK;
}

static audio_element_err_t _spdif_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait,
                                        void *context) {
  SPDIFStreamWriter *this_writer = (SPDIFStreamWriter *) audio_element_getdata(self);
  int bytes_written = 0;
  if (len) {
    esp_err_t ret = spdif_write(buffer, len, ticks_to_wait);
    if (ret == ESP_OK) {
      // esph_log_d(TAG, "written: %d", len );
      bytes_written = len;
    } else {
      esph_log_e(TAG, "error writing to spdif stream %d", ret);
    }
  }
  return (audio_element_err_t) bytes_written;
}

static audio_element_err_t _adf_process(audio_element_handle_t self, char *in_buffer, int in_len) {
  // esph_log_d(TAG, "_adf_process: %d", in_len );
  int r_size = audio_element_input(self, in_buffer, in_len);
  int w_size = 0;

  if (r_size > 0) {
    w_size = audio_element_output(self, in_buffer, r_size);
    audio_element_update_byte_pos(self, w_size);
  }
  return (audio_element_err_t) w_size;
}

bool SPDIFStreamWriter::init_adf_elements_() {
  esph_log_d(TAG, "init_adf_elements_");
  if (this->sdk_audio_elements_.size() > 0)
    return true;

  audio_element_cfg_t cfg{};
  cfg.open = _spdif_open;
  cfg.seek = nullptr;
  cfg.process = _adf_process;
  cfg.close = _spdif_close;
  cfg.destroy = nullptr;
  cfg.write = _spdif_write;

  cfg.buffer_len = CHUNK_SIZE;
  cfg.task_stack = 2 * DEFAULT_ELEMENT_STACK_SIZE;  //-1; //3072+512;
  cfg.task_prio = 5;
  cfg.task_core = 0;
  cfg.out_rb_size = 4 * CHUNK_SIZE;
  cfg.data = nullptr;
  cfg.tag = "spdifwriter";
  cfg.stack_in_ext = true;
  cfg.multi_out_rb_num = 0;
  cfg.multi_in_rb_num = 0;

  this->spdif_audio_stream_ = audio_element_init(&cfg);
  audio_element_setdata(this->spdif_audio_stream_, this);

  sdk_audio_elements_.push_back(this->spdif_audio_stream_);
  sdk_element_tags_.push_back("spdif_out");

  spdif_init(sample_rate_);

  return true;
}

bool SPDIFStreamWriter::preparing_step() {
  esph_log_d(TAG, "preparing_step");
  audio_element_state_t curr_state = audio_element_get_state(this->spdif_audio_stream_);
  // esph_log_d(TAG, "SPDIF status: %d", curr_state);
  if (curr_state == AEL_STATE_RUNNING) {
    audio_element_pause(this->spdif_audio_stream_);
  } else if (curr_state != AEL_STATE_PAUSED) {
    if (audio_element_run(this->spdif_audio_stream_) != ESP_OK) {
      esph_log_e(TAG, "Starting SPDIF stream element failed");
    }
    if (audio_element_pause(this->spdif_audio_stream_)) {
      esph_log_e(TAG, "Pausing SPDIF stream element failed");
    }
  }
  return true;
}

bool SPDIFStreamWriter::is_ready() {
  esph_log_d(TAG, "is_ready");
  return true;
}

void SPDIFStreamWriter::reset_() { esph_log_d(TAG, "reset_"); }

void SPDIFStreamWriter::clear_adf_elements_() {
  esph_log_d(TAG, "clear_adf_elements_");
  this->sdk_audio_elements_.clear();
  this->sdk_element_tags_.clear();
}

void SPDIFStreamWriter::on_settings_request(AudioPipelineSettingsRequest &request) {
  esph_log_d(TAG, "on_settings_request");
  if (!this->spdif_audio_stream_) {
    return;
  }

  if (request.sampling_rate > 0 && (uint32_t) request.sampling_rate != this->sample_rate_) {
    this->sample_rate_ = request.sampling_rate;
    esph_log_d(TAG, "Setting sample rate to %d", this->sample_rate_);
    spdif_set_sample_rates(this->sample_rate_);
  }

  if (request.final_sampling_rate == -1) {
    request.final_sampling_rate = sample_rate_;
    request.final_bit_depth = 16;
    request.final_number_of_channels = 2;
  } else if (request.final_sampling_rate != sample_rate_ || request.final_sampling_rate != sample_rate_ ||
             request.final_bit_depth != 16 || request.final_number_of_channels != 2) {
    request.failed = true;
    request.failed_by = this;
  }
}

}  // namespace spdif_audio
}  // namespace esphome
#endif
