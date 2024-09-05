#pragma once

#ifdef USE_ESP_IDF
#include "esphome/core/component.h"

#include "esphome/components/adf_pipeline/adf_audio_sinks.h"

namespace esphome {
using namespace esp_adf;
namespace spdif_audio {

class SPDIFStreamWriter : public ADFPipelineSinkElement, public Component {
 public:
  const std::string get_name() override { return "SPDIF-Audio-Out"; }
  bool is_ready() override;
  bool preparing_step();

 protected:
  bool init_adf_elements_() override;
  void clear_adf_elements_() override;
  void reset_() override;
  void on_settings_request(AudioPipelineSettingsRequest &request) override;

  static esp_err_t _spdif_open(audio_element_handle_t self);
  static esp_err_t _spdif_close(audio_element_handle_t self);
  static audio_element_err_t _spdif_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait,
                                          void *context);
  static audio_element_err_t _adf_process(audio_element_handle_t self, char *in_buffer, int in_len);

  audio_element_handle_t spdif_audio_stream_{nullptr};
  uint16_t sample_rate_{48000};
  float volume_{1.0f};
};

}  // namespace spdif_audio
}  // namespace esphome

#endif
