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

#include "gazebo_sim/include/gazebo_plugin.hpp"

namespace rosflight_sim
{

GazeboPlugin::GazeboPlugin(std::function<void()> on_update_callback)
  : gazebo::ModelPlugin()
  , on_update_callback_ptr_(on_update_callback)
{}

GazeboPlugin::~GazeboPlugin() { GZ_COMPAT_DISCONNECT_WORLD_UPDATE_BEGIN(updateConnection_); }

void GazeboPlugin::Load(gazebo::physics::ModelPtr _model, sdf::ElementPtr _sdf)
{
  node_ = gazebo_ros::Node::Get(_sdf);
  model_ = _model;
  world_ = model_->GetWorld();

  /*
   * Connect the Plugin to the Robot and save pointers to the various elements in the simulation
   */
  if (_sdf->HasElement("linkName")) {
    link_name_ = _sdf->GetElement("linkName")->Get<std::string>();
  } else {
    gzerr << "[ROSflight_SIL] Please specify a linkName of the forces and moments plugin.\n";
  }
  link_ = model_->GetLink(link_name_);
  if (link_ == nullptr) {
    gzthrow("[ROSflight_SIL] Couldn't find specified link \"" << link_name_ << "\".")
  }

  /* Load Params from Gazebo Server */
  if (_sdf->HasElement("mavType")) {
    mav_type_ = _sdf->GetElement("mavType")->Get<std::string>();
  } else {
    mav_type_ = "multirotor";
    gzerr << "[rosflight_sim] Please specify a value for parameter \"mavType\".\n";
  }

  // Declare the initial pose for the reset method
  initial_pose_ = GZ_COMPAT_GET_WORLD_COG_POSE(link_);

  declare_SIL_params();

  // Connect the update function to the simulation
  updateConnection_ =
    gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboPlugin::OnUpdate, this, _1));
}

void GazeboPlugin::declare_SIL_params()
{
  node_->declare_parameter("gazebo_host", rclcpp::PARAMETER_STRING);
  node_->declare_parameter("gazebo_port", rclcpp::PARAMETER_INTEGER);
  node_->declare_parameter("ROS_host", rclcpp::PARAMETER_STRING);
  node_->declare_parameter("ROS_port", rclcpp::PARAMETER_INTEGER);

  node_->declare_parameter("serial_delay_ns", rclcpp::PARAMETER_INTEGER);
}

void GazeboPlugin::OnUpdate(const gazebo::common::UpdateInfo & _info)
{
  // TODO: should we add the _info.simTime.Double() to the callback ptr? 
  on_update_callback_ptr_();
}

void GazeboPlugin::Reset()
{
  link_->SetWorldPose(initial_pose_);
  link_->ResetPhysicsStates();
}

Eigen::Vector3d GazeboPlugin::vec3_to_eigen_from_gazebo(GazeboVector vec)
{
  Eigen::Vector3d out;
  out << GZ_COMPAT_GET_X(vec), GZ_COMPAT_GET_Y(vec), GZ_COMPAT_GET_Z(vec);
  return out;
}

GazeboVector GazeboPlugin::vec3_to_gazebo_from_eigen(Eigen::Vector3d vec)
{
  GazeboVector out(vec(0), vec(1), vec(2));
  return out;
}

Eigen::Matrix3d GazeboPlugin::rotation_to_eigen_from_gazebo(GazeboQuaternion quat)
{
  Eigen::Quaterniond eig_quat(GZ_COMPAT_GET_W(quat), GZ_COMPAT_GET_X(quat), GZ_COMPAT_GET_Y(quat),
                              GZ_COMPAT_GET_Z(quat));
  return eig_quat.toRotationMatrix();
}

GZ_REGISTER_MODEL_PLUGIN(GazeboPlugin)
} // rosflight_sim
