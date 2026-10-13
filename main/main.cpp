/*
 * SPDX-FileCopyrightText: 2026 Dirk Wahrheit
 * SPDX-License-Identifier: Apache-2.0
 *
 * espos-nmea-gateway: CAN frames off the boat's NMEA 2000 bus, out over TCP in
 * candump ASCII, which signalk-server reads with canboatjs. Frames the server
 * sends back go onto the bus, unless the "can" settings say receive only.
 *
 * The device decodes nothing: PGN decoding changes far more often than
 * firmware should, so canboat owns it on the server and this stays a pipe.
 * Everything of substance lives in espOS (espos/components/espos_n2k); this
 * file chooses the pins and wires the receiver, transmitter and server
 * together, as espOS's n2k_candump example does.
 */
#include <cinttypes>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_private/esp_gpio_reserve.h"
#include "espos.h"
#include "espos_cfg_keys.h"
#include "espos_config.h"
#include "espos_n2k/candump_tcp_server.h"
#include "espos_n2k/twai_receiver.h"
#include "espos_n2k/twai_transmitter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

const char* TAG = "nmea_gateway";

/* NMEA 2000 is 250 kbit/s, and not negotiable: a device at the wrong rate
 * corrupts frames for everyone else by acknowledging at the wrong time. */
constexpr uint32_t kBitrate = 250000;

/* A pin from the settings, or the build's default for this chip when the
 * setting is -1. GPIO_NUM_NC when the pin cannot be used, which espos_n2k
 * answers by leaving the bus stopped with one log line.
 *
 * Pins the flash or PSRAM already use are refused rather than handed to the
 * TWAI driver: rerouting one of those takes the flash away from the CPU, and a
 * device that crashes at every boot can no longer be reached to put the
 * setting back. */
gpio_num_t pin_from_config(const char* key, int build_default, bool output) {
  int32_t pin = -1;
  if (espos_config_get_i32(ESPOS_CFG_NS_CAN, key, &pin) != ESP_OK || pin < 0) {
    pin = build_default;
  }
  const bool valid = output ? GPIO_IS_VALID_OUTPUT_GPIO(pin) : GPIO_IS_VALID_GPIO(pin);
  if (!valid) {
    ESP_LOGE(TAG, "can.%s: GPIO %" PRId32 " cannot be a CAN %s pin on this chip", key, pin,
             output ? "TX" : "RX");
    return GPIO_NUM_NC;
  }
  if (esp_gpio_is_reserved(BIT64(pin))) {
    ESP_LOGE(TAG, "can.%s: GPIO %" PRId32 " belongs to the flash or PSRAM", key, pin);
    return GPIO_NUM_NC;
  }
  return static_cast<gpio_num_t>(pin);
}

}  // namespace

extern "C" void app_main(void) {
  /* log -> config -> httpd -> wifi -> sk -> ota, in the one order that works.
   * The bus does not depend on the network, so frames are counted from the
   * moment the transceiver sees traffic, server or not. */
  ESP_ERROR_CHECK(espos_start(nullptr));

  gpio_num_t tx_pin = pin_from_config(ESPOS_CFG_CAN_TX_PIN, CONFIG_NMEA_GW_CAN_TX_GPIO, true);
  gpio_num_t rx_pin = pin_from_config(ESPOS_CFG_CAN_RX_PIN, CONFIG_NMEA_GW_CAN_RX_GPIO, false);
  if (tx_pin != GPIO_NUM_NC && tx_pin == rx_pin) {
    ESP_LOGE(TAG, "CAN TX and RX are both GPIO %d", (int)tx_pin);
    tx_pin = rx_pin = GPIO_NUM_NC;
  }
  bool transmit = true;
  espos_config_get_bool(ESPOS_CFG_NS_CAN, ESPOS_CFG_CAN_TRANSMIT, &transmit);

  /* static: the server keeps pointers to these, and all three outlive
   * app_main(). The device runs until it reboots. */
  static espos_n2k::TwaiReceiver rx({
      .tx_pin = tx_pin,
      .rx_pin = rx_pin,
      .bitrate = kBitrate,
  });
  static espos_n2k::TwaiTransmitter tx;

  /* The receiver owns the bus; the transmitter attaches to what the receiver
   * brought up, so it starts second. */
  rx.start();
  if (transmit) {
    tx.start();
  }

  /* nullptr for the transmitter is what makes the gateway receive only: lines
   * a client sends are dropped instead of written to the bus. */
  static espos_n2k::CandumpTcpServer server(&rx, transmit ? &tx : nullptr, {});
  server.start();

  ESP_LOGI(TAG, "CAN on TX=%d RX=%d at %u kbit/s, %s; candump on port %d",
           (int)tx_pin, (int)rx_pin, (unsigned)(kBitrate / 1000),
           transmit ? "sending and receiving" : "receive only",
           (int)CONFIG_ESPOS_N2K_CANDUMP_PORT);
  ESP_LOGI(TAG, "bus health: GET /api/v1/n2k");

  /* Frames are handled on the TWAI node's task. This loop only narrates, and
   * only when something changes: once when the bus comes alive, once per
   * quiet spell, once when it recovers. */
  bool announced = false;
  bool warned_quiet = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    if (!announced && rx.ever_received()) {
      announced = true;
      ESP_LOGI(TAG, "bus is live: %u frames", (unsigned)rx.frames_received());
    }
    const int64_t idle = rx.seconds_since_last_rx();
    if (announced && idle > 30) {
      if (!warned_quiet) {
        warned_quiet = true;
        ESP_LOGW(TAG, "no frames for %lld s -- check the bus", (long long)idle);
      }
    } else if (warned_quiet) {
      warned_quiet = false;
      ESP_LOGI(TAG, "frames again after a quiet spell");
    }
  }
}
