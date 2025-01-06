/*
 * Software License Agreement (BSD-3 License)
 *
 * Copyright (c) 2017 Daniel Koch, James Jackson and Gary Ellingson, BYU MAGICC Lab.
 * Copyright (c) 2023 Brandon Sutherland, AeroVironment Inc.
 * Copyright (c) 2024 Ian Reid, BYU MAGICC Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <chrono>
#include <cmath>
#include <fstream>

#include <rclcpp/logging.hpp>

#include "rosflight_sim/sil_board.hpp"

namespace rosflight_sim
{
SILBoard::SILBoard(rclcpp::Node::SharedPtr node)
    : UDPBoard()
    // , bias_generator_(std::chrono::system_clock::now().time_since_epoch().count()) // Uncomment if you would like to
                                                                                      // have bias biases for the sensors
                                                                                      // on each flight. Delete next line.
    , bias_generator_(0)
    , noise_generator_(std::chrono::system_clock::now().time_since_epoch().count())
    , node_(node)
{}

void SILBoard::init_board() {
  // TODO: Time Manager
  //boot_time_ = GZ_COMPAT_GET_SIM_TIME(world_);
  boot_time_ = 0.0;

  // Set up the udp connection
  // TODO: rename these to simulation_host instead of gazebo
  auto bind_host = node_->get_parameter_or<std::string>("gazebo_host", "localhost");
  int bind_port = node_->get_parameter_or<int>("gazebo_port", 14525);
  auto remote_host = node_->get_parameter_or<std::string>("ROS_host", "localhost");
  int remote_port = node_->get_parameter_or<int>("ROS_port", 14520);

  set_ports(bind_host, bind_port, remote_host, remote_port);
  gzmsg << "ROSflight SIL Conneced to " << remote_host << ":" << remote_port << " from "
        << bind_host << ":" << bind_port << "\n";

}

constexpr double rad2Deg(double x) { return 180.0 / M_PI * x; }
constexpr double deg2Rad(double x) { return M_PI / 180.0 * x; }

// clock
// TODO: Time manager 
uint32_t SILBoard::clock_millis()
{
  uint32_t millis = (uint32_t) ((GZ_COMPAT_GET_SIM_TIME(world_) - boot_time_).Double() * 1e3);
  return millis;
}

uint64_t SILBoard::clock_micros()
{
  uint64_t micros = (uint64_t) ((GZ_COMPAT_GET_SIM_TIME(world_) - boot_time_).Double() * 1e6);
  return micros;
}

uint8_t SILBoard::serial_read()
{
  auto next_message = serial_delay_queue_.front();
  serial_delay_queue_.pop();
  return std::get<1>(next_message);
}

uint16_t SILBoard::serial_bytes_available()
{
  // Get current time. Doesn't use ROS time as ROS time proved to be inconsistent and lead to slower
  // serial communication.
  auto current_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();

  // Get available serial_read messages from the firmware
  if (UDPBoard::serial_bytes_available()) {
    serial_delay_queue_.emplace(current_time, UDPBoard::serial_read());
  }

  // Determine if there are any serial_read messages that are ready for processing
  return !serial_delay_queue_.empty()
    && (current_time - std::get<0>(serial_delay_queue_.front())) > serial_delay_ns_;
}

// sensors
/// noise params are hard coded
void SILBoard::sensors_init()
{
  // Initialize subscribers
  imu_data_sub_ = node_->create_subscription<sensor_msgs::msg::Imu>("simulated_sensors/imu/data",
      std::bind(&SILBoard::imu_data_callback, this, std::placeholders::_1));

  imu_temperature_data_sub_ = node_->create_subscription<sensor_msgs::msg::Imu>("simulated_sensors/imu/temperature",
      std::bind(&SILBoard::imu_temperature_data_callback, this, std::placeholders::_1));

  mag_data_sub_ = node_->create_subscription<sensor_msgs::msg::MagneticField>("simulated_sensors/mag",
      std::bind(&SILBoard::mag_data_callback, this, std::placeholders::_1));

  baro_data_sub_ = node_->create_subscription<rosflight_msgs::msg::Barometer>("simulated_sensors/baro",
      std::bind(&SILBoard::baro_data_callback, this, std::placeholders::_1));

  gnss_data_sub_ = node_->create_subscription<rosflight_msgs::msg::GNSS>("simulated_sensors/gnss",
      std::bind(&SILBoard::gnss_data_callback, this, std::placeholders::_1));

  gnss_full_data_sub_ = node_->create_subscription<rosflight_msgs::msg::GNSSFull>("simulated_sensors/gnss_full",
      std::bind(&SILBoard::gnss_data_callback, this, std::placeholders::_1));

  diff_pressure_data_sub_ = node_->create_subscription<rosflight_msgs::msg::Airspeed>("simulated_sensors/diff_pressure",
      std::bind(&SILBoard::diff_pressure_data_callback, this, std::placeholders::_1));

  sonar_data_sub_ = node_->create_subscription<sensor_msgs::msg::Range>("simulated_sensors/sonar",
      std::bind(&SILBoard::sonar_data_callback, this, std::placeholders::_1));

  battery_data_sub_ = node_->create_subscription<rosflight_msgs::msg::BatteryStatus>("simulated_sensors/battery",
      std::bind(&SILBoard::battery_data_callback, this, std::placeholders::_1));
}

void SILBoard::imu_data_callback(const sensor_msgs::msg::Imu & msg)
{
  imu_data_ = msg;
  new_imu_data_available_ = true;
  imu_present_ = true;
}

void SILBoard::imu_temperature_data_callback(const sensor_msgs::msg::Temperature & msg)
{
  imu_temperature_data_ = msg;
  new_imu_temperature_data_available_ = true;
  imu_present_ = true;
}

void SILBoard::mag_data_callback(const sensor_msgs::msg::MagneticField & msg)
{
  mag_data_ = msg;
  new_mag_data_available_ = true;
  mag_present_ = true;
}

void SILBoard::baro_data_callback(const rosflight_msgs::msg::Barometer & msg)
{
  baro_data_ = msg;
  new_baro_data_available_ = true;
  baro_present_ = true;
}

void SILBoard::gnss_data_callback(const rosflight_msgs::msg::GNSS & msg)
{
  gnss_data_ = msg;
  new_gnss_data_available_ = true;
  gnss_present_ = true;
}

void SILBoard::gnss_full_data_callback(const rosflight_msgs::msg::GNSSFull & msg)
{
  gnss_full_data_ = msg;
  new_gnss_full_data_available_ = true;
  gnss_present_ = true;
}

void SILBoard::sonar_data_callback(const sensor_msgs::msg::Range & msg)
{
  sonar_data_ = msg;
  new_sonar_data_available_ = true;
  sonar_present_ = true;
}

void SILBoard::battery_data_callback(const rosflight_msgs::msg::BatteryStatus & msg)
{
  battery_data_ = msg;
  new_battery_data_available_ = true;
  battery_present_ = true;
}

bool SILBoard::imu_has_new_data()
{
  if (new_imu_data_available_ && new_imu_temperature_data_available_) {
    new_imu_data_available_ = false;
    new_imu_temperature_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::mag_has_new_data()
{
  if (new_mag_data_available_) {
    new_mag_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::gnss_has_new_data()
{
  if (new_gnss_data_available_ && new_gnss_full_data_available_) {
    new_gnss_data_available_ = false;
    new_gnss_full_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::baro_has_new_data()
{
  if (new_baro_data_available_) {
    new_baro_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::diff_pressure_has_new_data()
{
  if (new_diff_pressure_data_available_) {
    new_diff_pressure_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::sonar_has_new_data()
{
  if (new_sonar_data_available_) {
    new_sonar_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::rc_has_new_data()
{
  if (new_rc_data_available_) {
    new_rc_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::battery_has_new_data()
{
  if (new_battery_data_available_) {
    new_battery_data_available_ = false;
    return true;
  } else {
    return false;
  }
}

bool SILBoard::imu_present() { return imu_present_; }
bool SILBoard::mag_present() { return mag_present_; }
bool SILBoard::baro_present() { return baro_present_; }
bool SILBoard::diff_pressure_present() { return diff_pressure_present_; }
bool SILBoard::sonar_present() { return sonar_present_; }
bool SILBoard::battery_present() { return battery_present_; }
bool SILBoard::gnss_present() { return gnss_present_; }

bool SILBoard::imu_read(float accel[3], float * temperature, float gyro[3], uint64_t * time_us)
{
  // Populate the data from the last imu data received by the sensor
  accel[0] = imu_data_.linear_acceleration.x;
  accel[1] = imu_data_.linear_acceleration.y;
  accel[2] = imu_data_.linear_acceleration.z;

  (*temperature) = imu_temperature_data_.temperature + 273.15;  // Convert to degrees Kelvin from Celcius

  gyro[0] = imu_data_.angular_velocity.x;
  gyro[1] = imu_data_.angular_velocity.y;
  gyro[2] = imu_data_.angular_velocity.z;

  (*time_us) = imu_data_.header.stamp.sec + imu_data_.header.stamp.nanosec * 1e-9;
  return true;
}

void SILBoard::imu_not_responding_error()
{
  RCLCPP_ERROR(node_->get_logger(), "[gazebo_rosflight_sil] imu not responding");
}

bool SILBoard::mag_read(float mag[3])
{
  // TODO: should this be in tesla or nanotesla?
  mag[0] = mag_data_.magnetic_field.x;
  mag[1] = mag_data_.magnetic_field.y;
  mag[2] = mag_data_.magnetic_field.z;
  return true;
}

bool SILBoard::baro_read(float * pressure, float * temperature)
{
  (*pressure) = baro_data_.pressure;
  (*temperature) = baro_data_.temperature;
  return true;
}

bool SILBoard::diff_pressure_read(float * diff_pressure, float * temperature)
{
  (*diff_pressure) = diff_pressure_data_.differential_pressure;
  (*temperature) = diff_pressure_data_.temperature;
  return true;
}

bool SILBoard::sonar_read(float * range)
{
  (*range) = sonar_data_.range;
  return true
}

bool SILBoard::battery_read(float * voltage, float * current)
{
  (*voltage) = battery_data_.voltage;
  (*current) = battery_data_.current;
  return true;
}

void SILBoard::battery_voltage_set_multiplier(double multiplier)
{
  battery_voltage_multiplier_ = (float) multiplier;
}

void SILBoard::battery_current_set_multiplier(double multiplier)
{
  battery_current_multiplier_ = (float) multiplier;
}

// PWM
void SILBoard::pwm_init(uint32_t refresh_rate, uint16_t idle_pwm)
{
  rc_received_ = false;
  latestRC_.values[0] = 1500; // x
  latestRC_.values[1] = 1500; // y
  latestRC_.values[3] = 1500; // z
  latestRC_.values[2] = 1000; // F
  latestRC_.values[4] = 1000; // attitude override
  latestRC_.values[5] = 1000; // arm

  for (int & pwm_output : pwm_outputs_) {
    pwm_output = 1000;
  }

  rc_sub_ = node_->create_subscription<rosflight_msgs::msg::RCRaw>(
    "RC", 1, std::bind(&SILBoard::RC_callback, this, std::placeholders::_1));
}

void SILBoard::pwm_init_multi(const float *rate, uint32_t channels)
{
  // Only call it once, since we don't set the rate for each channel differently in the SIM board.
  // This works since the pwm_init doesn't use the arguments passed to it (in the SIL board)
  pwm_init(0, 0);
}

float SILBoard::rc_read(uint8_t channel)
{
  if (rc_sub_->get_publisher_count() > 0) {
    return static_cast<float>(latestRC_.values[channel] - 1000) / 1000.0f;
  }

  // no publishers, set throttle low and center everything else
  if (channel == 2) {
    return 0.0;
  }

  return 0.5;
}

void SILBoard::pwm_write(uint8_t channel, float value)
{
  pwm_outputs_[channel] = 1000 + (uint16_t) (1000 * value);
}

void SILBoard::pwm_write_multi(float *value, uint32_t channels)
{
  for (int i=0; i<(int) channels; ++i) {
    pwm_write(i, value[i]);
  }
}

void SILBoard::pwm_disable()
{
  for (int i = 0; i < 14; i++) {
    pwm_write(i, 0);
  }
}

bool SILBoard::rc_lost() { return !rc_received_; }

// non-volatile memory
bool SILBoard::memory_read(void * dest, size_t len)
{
  std::string directory = "rosflight_memory" + std::string(node_->get_namespace());
  std::ifstream memory_file;
  memory_file.open(directory + "/mem.bin", std::ios::binary);

  if (!memory_file.is_open()) {
    RCLCPP_ERROR(node_->get_logger(), "Unable to load rosflight memory file %s/mem.bin",
                 directory.c_str());
    return false;
  }

  memory_file.read((char *) dest, (long) len);
  memory_file.close();
  return true;
}

bool SILBoard::memory_write(const void * src, size_t len)
{
  std::string directory = "rosflight_memory" + std::string(node_->get_namespace());
  std::string mkdir_command = "mkdir -p " + directory;
  const int dir_err = system(mkdir_command.c_str());

  if (dir_err == -1) {
    RCLCPP_ERROR(node_->get_logger(), "Unable to write rosflight memory file %s/mem.bin",
                 directory.c_str());
    return false;
  }

  std::ofstream memory_file;
  memory_file.open(directory + "/mem.bin", std::ios::binary);
  memory_file.write((char *) src, (long) len);
  memory_file.close();
  return true;
}

bool SILBoard::motors_spinning()
{
  if (pwm_outputs_[2] > 1100) {
    return true;
  } else {
    return false;
  }
}

bool SILBoard::backup_memory_read(void * dest, size_t len)
{
  if (len <= BACKUP_SRAM_SIZE) {
    memcpy(dest, backup_memory_, len);
    return true;
  } else {
    return false;
  }
}

void SILBoard::backup_memory_write(const void * src, size_t len)
{
  if (len < BACKUP_SRAM_SIZE) {
    memcpy(backup_memory_, src, len);
  }
}

void SILBoard::backup_memory_clear(size_t len)
{
  if (len < BACKUP_SRAM_SIZE) {
    memset(backup_memory_, 0, len);
  }
}

void SILBoard::RC_callback(const rosflight_msgs::msg::RCRaw & msg)
{
  rc_received_ = true;
  new_rc_data_available_ = true;
  last_rc_message_ = node_->get_clock()->now();
  latestRC_ = msg;
}

bool SILBoard::gnss_read(rosflight_firmware::GNSSData * gnss,
                         rosflight_firmware::GNSSFull * gnss_full)
{
  // The commented out data doesn't get used by ROSflightIO
  // TODO: Just kidding it does.... Let's change the GNSS messages.
  // gnss->lat = gnss_data_.lat;
  // gnss->lon = gnss_data_.lon;
  // gnss->height = gnss_.height;

  // gnss->vel_n = gnss_data_.velocity[0];
  // gnss->vel_e = gnss_data_.velocity[1];
  // gnss->vel_d = gnss_data_.velocity[2];

  gnss->fix_type = std::static_cast<rosflight_firmware::GNSSFixType>(gnss_data_.fix);
  // gnss->time_of_week = gnss_.time_of_week;
  gnss->time = gnss_data_.header.stamp.sec;
  gnss->nanos = gnss_data_.header.stamp.nanos;

  gnss->h_acc = gnss_data_.horizontal_accuracy;
  gnss->v_acc = gnss_data_.vertical_accuracy;

  gnss->ecef.x = gnss_data_.position[0];
  gnss->ecef.y = gnss_data_.position[1];
  gnss->ecef.z = gnss_data_.position[2];
  // gnss->ecef.p_acc = gnss_.ecef.p_acc;
  gnss->ecef.vx = gnss_data_.velocity[0];
  gnss->ecef.vy = gnss_data_.velocity[1];
  gnss->ecef.vz = gnss_data_.velocity[2];
  gnss->ecef.s_acc = gnss_data_.speed_accuracy;

  gnss->rosflight_timestamp = (gnss_data_.header.stamp.sec + gnss_data_.header.stamp.nanosec * 1e-9) * 1000;  // Convert to millis

  gnss_full->year = gnss_full_.year;
  gnss_full->month = gnss_full_.month;
  gnss_full->day = gnss_full_.day;
  gnss_full->hour = gnss_full_.hour;
  gnss_full->min = gnss_full_.min;
  gnss_full->sec = gnss_full_.sec;
  gnss_full->valid = gnss_full_.valid;

  gnss_full->lat = gnss_full_.lat;
  gnss_full->lon = gnss_full_.lon;
  gnss_full->height = gnss_full_.height;
  gnss_full->height_msl = gnss_full_.height_msl;

  // For now, we have defined the Gazebo Local Frame as NWU.  This should be
  // fixed in a future commit
  gnss_full.vel_n = gnss_full_.vel_n;
  gnss_full.vel_e = gnss_full_.vel_e;
  gnss_full.vel_d = gnss_full_.vel_d;

  gnss_full.fix_type = gnss_full_.fix_type;
  gnss_full.time_of_week = gnss_full_.time_of_week;
  gnss_full.num_sat = gnss_full_.num_sat;
  // TODO
  gnss_full.t_acc = gnss_full_.t_acc;
  gnss_full.nano = gnss_full_.nano;

  gnss_full.h_acc = gnss_full_.h_acc;
  gnss_full.v_acc = gnss_full_.v_acc;

  gnss_full.g_speed = gnss_full_.g_speed;

  gnss_full.head_mot = gnss_full_.head_mot;
  gnss_full.p_dop = gnss_full_.p_dop;
  gnss_full.rosflight_timestamp = (gnss_full_data_.header.stamp.sec + gnss_full_data_.header.stamp.nanosec * 1e-9) * 1000;  // Convert to millis

  return true;
}

} // namespace rosflight_sim
