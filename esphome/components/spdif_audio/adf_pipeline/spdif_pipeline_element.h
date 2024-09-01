#pragma once

#ifdef USE_ESP_IDF
#include "esphome/core/component.h"
#include "esphome/core/log.h"

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

  audio_element_handle_t spdif_audio_stream_{nullptr};
};

}  // namespace spdif_audio
}  // namespace esphome

#endif
