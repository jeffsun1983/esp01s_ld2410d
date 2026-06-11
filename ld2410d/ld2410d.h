#pragma once

#include <map>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410d {

class LD2410DComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // 设置参数（写入传感器）
  void set_motion_threshold(uint8_t gate, float value_db);
  void set_micro_threshold(uint8_t gate, float value_db);
  void set_max_distance(float distance_meters);
  void set_timeout_seconds(uint16_t timeout_seconds);
  void save_config();

  // 读取参数（用于启动同步）
  void sync_all_configs();

  // 注册 Number 实体
  void register_motion_number(uint8_t gate, number::Number *num);
  void register_micro_number(uint8_t gate, number::Number *num);
  void register_distance_number(number::Number *num);
  void register_timeout_number(number::Number *num);

 protected:
  bool enter_config_mode();
  bool exit_config_mode();
  void send_command(uint16_t cmd, const std::vector<uint8_t> &data);
  std::vector<uint8_t> read_response(uint32_t timeout_ms = 200);
  void flush_input();
  bool read_parameter_internal(uint16_t param_id, uint32_t &value);
  uint32_t db_to_raw(float db);
  float raw_to_db(uint32_t raw);

  bool config_mode_ = false;

  std::map<uint8_t, number::Number*> motion_numbers_;
  std::map<uint8_t, number::Number*> micro_numbers_;
  number::Number* distance_number_ = nullptr;
  number::Number* timeout_number_ = nullptr;
};

class LD2410DNumber : public number::Number, public Component {
 public:
  void set_parent(LD2410DComponent *parent) { parent_ = parent; }
  void set_gate(uint8_t gate) { gate_ = gate; }
  void set_type(const std::string &type) { type_ = type; }
  void setup() override;
  void control(float value) override;

 protected:
  LD2410DComponent *parent_;
  uint8_t gate_;
  std::string type_;
};

}  // namespace ld2410d
}  // namespace esphome