/**
 * @file xmpl100_basic_example.cpp
 * @brief Initialise the XMPL100 from the menuconfig values and log the temperature twice a second.
 *
 * Wiring: examples/esp32/README.md. Expected output: one "initialised" line with the silicon
 * revision, then "23.45 C" lines. Every Result is checked and logged by name.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_xmpl100_port.hpp"
#include "xmpl100.hpp"
#include "xmpl100_kconfig.hpp"

namespace {
constexpr const char* kTag = "xmpl100_example";
}

extern "C" void app_main(void) {
  static Esp32Xmpl100Port port; // static: the driver keeps a reference
  static xmpl100::XMPL100<Esp32Xmpl100Port> sensor(port, xmpl100::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "XMPL100 driver %s", xmpl100::GetDriverVersion());
  const auto init = sensor.EnsureInitialized();
  if (!init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", xmpl100::ToString(init.error));
    return;
  }
  ESP_LOGI(kTag, "initialised, silicon revision 0x%02X", sensor.GetRevision());

  while (true) {
    const auto t = sensor.ReadTemperatureC();
    if (t) {
      ESP_LOGI(kTag, "%.2f C", static_cast<double>(t.value));
    } else {
      ESP_LOGW(kTag, "read failed: %s", xmpl100::ToString(t.error));
      if (t.error == xmpl100::Error::Bus) {
        (void)sensor.SoftReset(); // recover after a bus glitch; next read reports again
      } else if (t.error == xmpl100::Error::UnexpectedReset) {
        (void)sensor.EnsureInitialized(); // the chip lost its configuration: start it again
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
