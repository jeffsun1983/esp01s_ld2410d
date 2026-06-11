#include "ld2410d.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2410d {

static const char *TAG = "ld2410d";
static const uint8_t FRAME_HEADER[] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t FRAME_FOOTER[] = {0x04, 0x03, 0x02, 0x01};

void LD2410DComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2410D...");
  delay(500);
  this->set_timeout(5000, [this]() {
    this->sync_all_configs();
  });
}

void LD2410DComponent::loop() {}

void LD2410DComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2410D custom component (stable)");
}

void LD2410DComponent::flush_input() {
  while (available()) {
    read();
  }
}

void LD2410DComponent::send_command(uint16_t cmd, const std::vector<uint8_t> &data) {
  flush_input();
  std::vector<uint8_t> frame;
  frame.insert(frame.end(), FRAME_HEADER, FRAME_HEADER + 4);
  uint16_t data_len = 2 + data.size();
  frame.push_back(data_len & 0xFF);
  frame.push_back((data_len >> 8) & 0xFF);
  frame.push_back(cmd & 0xFF);
  frame.push_back((cmd >> 8) & 0xFF);
  frame.insert(frame.end(), data.begin(), data.end());
  frame.insert(frame.end(), FRAME_FOOTER, FRAME_FOOTER + 4);
  write_array(frame.data(), frame.size());
  flush();
}

std::vector<uint8_t> LD2410DComponent::read_response(uint32_t timeout_ms) {
  std::vector<uint8_t> resp;
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (available()) {
      resp.push_back(read());
    }
    delay(2);
  }
  return resp;
}

bool LD2410DComponent::read_parameter_internal(uint16_t param_id, uint32_t &value) {
  std::vector<uint8_t> data;
  data.push_back(param_id & 0xFF);
  data.push_back((param_id >> 8) & 0xFF);
  send_command(0x0008, data);
  auto resp = read_response(300);
  if (resp.size() >= 12) {
    value = 0;
    for (int i = 0; i < 4; i++) {
      value |= ((uint32_t)resp[resp.size() - 8 + i]) << (8 * i);
    }
    return true;
  }
  return false;
}

bool LD2410DComponent::enter_config_mode() {
  if (config_mode_) return true;
  flush_input();
  send_command(0x00FF, {0x01, 0x00});
  read_response(300);
  config_mode_ = true;
  return true;
}

bool LD2410DComponent::exit_config_mode() {
  if (!config_mode_) return true;
  send_command(0x00FE, {});
  read_response(200);
  config_mode_ = false;
  return true;
}

void LD2410DComponent::sync_all_configs() {
  ESP_LOGI(TAG, "Syncing all configurations...");
  if (!enter_config_mode()) {
    ESP_LOGE(TAG, "Failed to enter config mode");
    return;
  }

  uint32_t raw_dist = 0;
  if (read_parameter_internal(0x0001, raw_dist)) {
    float dist = raw_dist / 10.0f;
    if (distance_number_) distance_number_->publish_state(dist);
    ESP_LOGD(TAG, "Max distance: %.1f m", dist);
  }

  uint32_t raw_timeout = 0;
  if (read_parameter_internal(0x0004, raw_timeout)) {
    uint16_t tout = (uint16_t)raw_timeout;
    if (timeout_number_) timeout_number_->publish_state(tout);
    ESP_LOGD(TAG, "Timeout: %u s", tout);
  }

  for (int g = 0; g <= 15; g++) {
    uint32_t raw = 0;
    if (read_parameter_internal(0x0010 + g, raw)) {
      float val = raw_to_db(raw);
      if (motion_numbers_.count(g) && motion_numbers_[g])
        motion_numbers_[g]->publish_state(val);
    }
    delay(10);
  }

  for (int g = 0; g <= 15; g++) {
    uint32_t raw = 0;
    if (read_parameter_internal(0x0030 + g, raw)) {
      float val = raw_to_db(raw);
      if (micro_numbers_.count(g) && micro_numbers_[g])
        micro_numbers_[g]->publish_state(val);
    }
    delay(10);
  }

  exit_config_mode();
  ESP_LOGI(TAG, "Sync completed.");
}

void LD2410DComponent::set_motion_threshold(uint8_t gate, float value_db) {
  if (gate > 15) return;
  enter_config_mode();
  uint32_t raw = db_to_raw(value_db);
  uint16_t param_id = 0x0010 + gate;
  std::vector<uint8_t> data;
  data.push_back(param_id & 0xFF);
  data.push_back((param_id >> 8) & 0xFF);
  for (int i = 0; i < 4; i++) data.push_back((raw >> (8 * i)) & 0xFF);
  send_command(0x0007, data);
  read_response(300);
  save_config();
  exit_config_mode();
  if (motion_numbers_.count(gate) && motion_numbers_[gate])
    motion_numbers_[gate]->publish_state(value_db);
}

void LD2410DComponent::set_micro_threshold(uint8_t gate, float value_db) {
  if (gate > 15) return;
  enter_config_mode();
  uint32_t raw = db_to_raw(value_db);
  uint16_t param_id = 0x0030 + gate;
  std::vector<uint8_t> data;
  data.push_back(param_id & 0xFF);
  data.push_back((param_id >> 8) & 0xFF);
  for (int i = 0; i < 4; i++) data.push_back((raw >> (8 * i)) & 0xFF);
  send_command(0x0007, data);
  read_response(300);
  save_config();
  exit_config_mode();
  if (micro_numbers_.count(gate) && micro_numbers_[gate])
    micro_numbers_[gate]->publish_state(value_db);
}

void LD2410DComponent::set_max_distance(float distance_meters) {
  uint16_t distance_raw = (uint16_t)(distance_meters * 10.0f);
  if (distance_raw < 7) distance_raw = 7;
  if (distance_raw > 100) distance_raw = 100;
  enter_config_mode();
  uint16_t param_id = 0x0001;
  std::vector<uint8_t> data;
  data.push_back(param_id & 0xFF);
  data.push_back((param_id >> 8) & 0xFF);
  data.push_back(distance_raw & 0xFF);
  data.push_back((distance_raw >> 8) & 0xFF);
  data.push_back(0x00);
  data.push_back(0x00);
  send_command(0x0007, data);
  read_response(300);
  save_config();
  exit_config_mode();
  if (distance_number_) distance_number_->publish_state(distance_meters);
}

void LD2410DComponent::set_timeout_seconds(uint16_t timeout_seconds) {
  enter_config_mode();
  uint16_t param_id = 0x0004;
  std::vector<uint8_t> data;
  data.push_back(param_id & 0xFF);
  data.push_back((param_id >> 8) & 0xFF);
  data.push_back(timeout_seconds & 0xFF);
  data.push_back((timeout_seconds >> 8) & 0xFF);
  data.push_back(0x00);
  data.push_back(0x00);
  send_command(0x0007, data);
  read_response(300);
  save_config();
  exit_config_mode();
  if (timeout_number_) timeout_number_->publish_state(timeout_seconds);
}

void LD2410DComponent::save_config() {
  send_command(0x00FD, {});
  read_response(200);
}

uint32_t LD2410DComponent::db_to_raw(float db) {
  return (uint32_t)(powf(10.0f, db / 10.0f));
}

float LD2410DComponent::raw_to_db(uint32_t raw) {
  return 10.0f * log10f(raw);
}

void LD2410DComponent::register_motion_number(uint8_t gate, number::Number *num) {
  motion_numbers_[gate] = num;
}
void LD2410DComponent::register_micro_number(uint8_t gate, number::Number *num) {
  micro_numbers_[gate] = num;
}
void LD2410DComponent::register_distance_number(number::Number *num) {
  distance_number_ = num;
}
void LD2410DComponent::register_timeout_number(number::Number *num) {
  timeout_number_ = num;
}

void LD2410DNumber::setup() {
  if (type_ == "motion") parent_->register_motion_number(gate_, this);
  else if (type_ == "micro") parent_->register_micro_number(gate_, this);
}

void LD2410DNumber::control(float value) {
  if (type_ == "motion") parent_->set_motion_threshold(gate_, value);
  else if (type_ == "micro") parent_->set_micro_threshold(gate_, value);
  else if (type_ == "distance") parent_->set_max_distance(value);
  else if (type_ == "timeout") parent_->set_timeout_seconds((uint16_t)value);
  publish_state(value);
}

}  // namespace ld2410d
}  // namespace esphome