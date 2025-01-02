/*
 * Software License Agreement (BSD-3 License)
 *
 * Copyright (c) 2024 Jacob Moore
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

namespace rosflight_sim
{

SensorInterface::SensorInterface()
  : rclcpp::Node("sensors")
{
  // Declare parameters
  declare_parameters();
  
  // Set up the parameter callback
  parameter_callback_handle_ = this->add_on_set_parameters_callback(
      std::bind(&SensorInterface::parameters_callback, this, std::placeholders::_1));

  // Initialize timers with the frequencies from the parameters
  imu_update_frequency_ = this->get_double("imu_update_frequency");
  mag_update_frequency_ = this->get_double("mag_update_frequency");
  baro_update_frequency_ = this->get_double("baro_update_frequency");
  gnss_update_frequency_ = this->get_double("gnss_update_frequency");
  sonar_update_frequency_ = this->get_double("sonar_update_frequency");
  diff_pressure_update_frequency_ = this->get_double("diff_pressure_update_frequency");
  battery_update_frequency_ = this->get_double("battery_update_frequency");

  imu_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / imu_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::imu_publish, this));
  mag_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / mag_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::mag_publish, this));
  baro_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / baro_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::baro_publish, this));
  gnss_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / gnss_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::gnss_publish, this));
  diff_pressure_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / diff_pressure_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::diff_pressure_publish, this));
  sonar_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / sonar_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::sonar_publish, this));
  battery_timer_ = this->create_wall_timer(
      std::chrono::microseconds(static_cast<long long>(1.0 / battery_update_frequency_ * 1'000'000)),
      std::bind(&SensorInterface::battery_publish, this));
}

void SensorInterface::declare_parameters()
{
  // Declare all ROS2 parameters here
  this->declare_parameter("imu_update_frequency", 400.0);
  this->declare_parameter("mag_update_frequency", 50.0);
  this->declare_parameter("baro_update_frequency", 100.0);
  this->declare_parameter("gnss_update_frequency", 10.0);
  this->declare_parameter("sonar_update_frequency", 20.0);
  this->declare_parameter("diff_pressure_update_frequency", 100.0);
  this->declare_parameter("battery_update_frequency", 200.0);
}

rcl_interfaces::msg::SetParametersResult
SensorInterface::parameters_callback(const std::vector<rclcpp::Parameter> & parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = false;
  result.reason = "One of the parameters given is not a parameter of the target node";

  for (const auto & param : parameters) {
    if (param.get_name() == "imu_update_frequency" ||
        param.get_name() == "mag_update_frequency" ||
        param.get_name() == "baro_update_frequency" ||
        param.get_name() == "gnss_update_frequency" ||
        param.get_name() == "diff_pressure_update_frequency" ||
        param.get_name() == "sonar_update_frequency" ||
        param.get_name() == "battery_update_frequency")
      reset_timers();
  }
}

void SensorInterface::reset_timers()
{
  double frequency = this->get_double("imu_update_frequency");
  if (frequency != imu_update_frequency_) {
    // Reset the timer
    imu_timer_->cancel();
    imu_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::imu_publish, this));

    imu_update_frequency_ = frequency;
  }

  frequency = this->get_double("mag_update_frequency");
  if (frequency != mag_update_frequency_) {
    // Reset the timer
    mag_timer_->cancel();
    mag_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::mag_publish, this));

    mag_update_frequency_ = frequency;
  }

  frequency = this->get_double("gnss_update_frequency");
  if (frequency != gnss_update_frequency_) {
    // Reset the timer
    gnss_timer_->cancel();
    gnss_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::gnss_publish, this));

    gnss_update_frequency_ = frequency;
  }

  frequency = this->get_double("baro_update_frequency");
  if (frequency != baro_update_frequency_) {
    // Reset the timer
    baro_timer_->cancel();
    baro_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::baro_publish, this));

    baro_update_frequency_ = frequency;
  }

  frequency = this->get_double("diff_pressure_update_frequency");
  if (frequency != diff_pressure_update_frequency_) {
    // Reset the timer
    diff_pressure_timer_->cancel();
    diff_pressure_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::diff_pressure_publish, this));

    diff_pressure_update_frequency_ = frequency;
  }

  frequency = this->get_double("sonar_update_frequency");
  if (frequency != sonar_update_frequency_) {
    // Reset the timer
    sonar_timer_->cancel();
    sonar_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::sonar_publish, this));

    sonar_update_frequency_ = frequency;
  }

  frequency = this->get_double("battery_update_frequency");
  if (frequency != battery_update_frequency_) {
    // Reset the timer
    battery_timer_->cancel();
    battery_timer_ = this->create_wall_timer(
        std::chrono::microseconds(static_cast<long long>(1.0 / frequency_ * 1'000'000)),
        std::bind(&SensorInterface::battery_publish, this));

    battery_update_frequency_ = frequency;
  }
}


void SensorInterface::imu_publish()
{
  
  imu_pub_->publish(msg);
}

} // rosflight_sim

int main(int argc, char** argv)
{
  // TODO: this won't compile since simulator interface is an abstract class
  // We'll probably have to move this main function to the folder for each simulator, so they can construct their own class.
  rclcpp::init(agrc, argv);
  rclcpp::spin(std::make_shared<rosflight_sim::SimulatorInterface>());
  return 0;
}
