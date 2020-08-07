#include "ble_switch.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace ble_client {

static const char *TAG = "ble_switch";

void BleClientSwitch::write_state(bool state) {
  this->parent_->set_enabled(state);
  this->publish_state(state);
}

void BleClientSwitch::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT:
      this->publish_state(this->parent_->enabled_);
      break;
    case ESP_GATTC_OPEN_EVT:
      this->node_state_ = espbt::ClientState::Established;
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      this->node_state_ = espbt::ClientState::Idle;
      this->publish_state(this->parent_->enabled_);
      break;
    default:
      break;
  }
}

void BleClientSwitch::dump_config() { LOG_SWITCH("", "BLE Client Switch", this); }

}  // namespace ble_client
}  // namespace esphome