#include "ble_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_client {

static const char *TAG = "ble_sensor";

uint32_t BLESensor::hash_base() { return 343459825UL; }

void BLESensor::loop() {}

void BLESensor::dump_config() {
  LOG_SENSOR("", "BLE Sensor", this);
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->char_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Descriptor UUID    : %s", this->descr_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Notifications      : %s", YESNO(this->notify_));
  LOG_UPDATE_INTERVAL(this);
}

void BLESensor::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Connected successfully!", this->get_name().c_str());
        break;
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "[%s] Disconnected!", this->get_name().c_str());
      this->status_set_warning();
      this->publish_state(NAN);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->sensor_handle_ = 0;
      auto chr = this->parent_->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        this->status_set_warning();
        this->publish_state(NAN);
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->char_uuid_.to_string().c_str());
        break;
      }
      this->sensor_handle_ = chr->handle_;
      if (this->descr_uuid_.get_uuid().len > 0) {
        auto descr = chr->get_descriptor(this->descr_uuid_);
        if (descr == nullptr) {
          this->status_set_warning();
          this->publish_state(NAN);
          ESP_LOGW(TAG, "No sensor descriptor found at service %s char %s descr %s",
                   this->service_uuid_.to_string().c_str(), this->char_uuid_.to_string().c_str(),
                   this->descr_uuid_.to_string().c_str());
          break;
        }
        this->sensor_handle_ = descr->handle_;
      }
      if (this->notify_) {
        auto status =
            esp_ble_gattc_register_for_notify(this->parent_->gattc_if_, this->parent_->remote_bda_, chr->handle_);
        if (status) {
          ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
        }
      } else {
        this->node_state_ = espbt::ClientState::Established;
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent_->conn_id_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->sensor_handle_) {
        this->status_clear_warning();
        this->publish_state((float) param->read.value[0]);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent_->conn_id_ || param->notify.handle != this->sensor_handle_)
        break;
      ESP_LOGI(TAG, "[%s] ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", this->get_name().c_str(),
               param->notify.handle, param->notify.value[0]);
      this->publish_state((float) param->notify.value[0]);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state_ = espbt::ClientState::Established;
      break;
    }
    default:
      break;
  }
}

void BLESensor::update() {
  if (this->node_state_ != espbt::ClientState::Established) {
    ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->get_name().c_str());
    return;
  }
  if (this->sensor_handle_ == 0) {
    ESP_LOGW(TAG, "[%s] Cannot poll, no service or characteristic found", this->get_name().c_str());
    return;
  }

  auto status = esp_ble_gattc_read_char(this->parent_->gattc_if_, this->parent_->conn_id_, this->sensor_handle_,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    this->status_set_warning();
    this->publish_state(NAN);
    ESP_LOGW(TAG, "[%s] Error sending read request for sensor, status=%d", this->get_name().c_str(), status);
  }
}

}  // namespace ble_client
}  // namespace esphome
#endif