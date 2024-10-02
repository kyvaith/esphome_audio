#include "spdif_pipeline_element.h"

#ifdef USE_ESP_IDF

#include "../../adf_pipeline/adf_pipeline.h"
#include "freertos/event_groups.h"
#include "../../adf_pipeline/sdk_ext.h"
#include "../spdif.h"
#include "esphome/core/log.h"

namespace esphome {
using namespace esp_adf;
namespace spdif_audio {

static const char *const TAG = "adf_spdif_audio";
// 192 stereo samples at 16-bit. One full SPDIF frame worth of audio.
static const uint16_t CHUNK_SIZE = 768;

// Must exist or the pipeline does not start
esp_err_t SPDIFStreamWriter::_spdif_open(audio_element_handle_t self) {
  // esph_log_d(TAG, "_spdif_open");
  return ESP_OK;
}

esp_err_t SPDIFStreamWriter::_spdif_close(audio_element_handle_t self) {
  // esph_log_d(TAG, "_spdif_close");
  return ESP_OK;
}

audio_element_err_t SPDIFStreamWriter::_spdif_write(audio_element_handle_t self, char *buffer, int len,
                                                    TickType_t ticks_to_wait, void *context) {
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

audio_element_err_t SPDIFStreamWriter::_adf_process(audio_element_handle_t self, char *in_buffer, int in_len) {
  // esph_log_d(TAG, "_adf_process: %d", in_len );
  SPDIFStreamWriter *this_writer = (SPDIFStreamWriter *) audio_element_getdata(self);
  int r_size = audio_element_input(self, in_buffer, in_len);
  int w_size = 0;

  if (r_size > 0) {
    if (this_writer->volume_ < 1.0f) {
      // Apply volume scaling
      int16_t *samples = reinterpret_cast<int16_t *>(in_buffer);
      int num_samples = r_size / sizeof(int16_t);
      for (int i = 0; i < num_samples; i++) {
        samples[i] = static_cast<int16_t>(samples[i] * this_writer->volume_);
      }
    }

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
  cfg.stack_in_ext = false;
  cfg.multi_out_rb_num = 0;
  cfg.multi_in_rb_num = 0;

  this->spdif_audio_stream_ = audio_element_init(&cfg);
  audio_element_setdata(this->spdif_audio_stream_, this);

  sdk_audio_elements_.push_back(this->spdif_audio_stream_);
  sdk_element_tags_.push_back("spdif_out");

  spdif_init(sample_rate_);

  return true;
}

void SPDIFStreamWriter::clear_adf_elements_() {
  esph_log_d(TAG, "clear_adf_elements_");
  spdif_deinit();
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

  if (request.target_volume > 0 && request.target_volume != this->volume_) {
    this->volume_ = request.target_volume;
    esph_log_d(TAG, "Setting volume to %.2f", this->volume_);
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
