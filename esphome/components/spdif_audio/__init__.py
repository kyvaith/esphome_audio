import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import esp32
from esphome import pins


CODEOWNERS = ["@johnboiles"]
DEPENDENCIES = ["esp32"]

spdif_audio_ns = cg.esphome_ns.namespace("spdif_audio")

CONF_FILL_SILENCE = "fill_silence"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required("pin"): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_FILL_SILENCE, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_ESP32_SPDIF_STREAM")
    cg.add_define("SPDIF_DATA_PIN", config["pin"])
    cg.add_define("SPDIF_FILL_SILENCE", config[CONF_FILL_SILENCE])
