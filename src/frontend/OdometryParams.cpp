/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   OdometryParams.cpp
 * @brief  Params for Odometry Sensor.
 * @author Marcus Abate
 */

#include "kimera-vio/frontend/OdometryParams.h"

#include <glog/logging.h>

#include "kimera-vio/utils/UtilsOpenCV.h"
#include "kimera-vio/utils/YamlParser.h"

namespace VIO {

OdometryParams::OdometryParams() : PipelineParams("Odometry params") {}

bool OdometryParams::parseYAML(const std::string& filepath) {
  YamlParser yaml_parser(filepath);

  std::vector<double> vector_pose;
  yaml_parser.getNestedYamlParam("T_BS", "data", &vector_pose);
  const gtsam::Pose3& body_Pose_cam =
      UtilsOpenCV::poseVectorToGtsamPose3(vector_pose);

  yaml_parser.getYamlParam("odomRotationPrecision", &betweenRotationPrecision_);
  yaml_parser.getYamlParam("odomPositionPrecision", &betweenTranslationPrecision_);
  yaml_parser.getYamlParam("odomVelPrecision", &velocityPrecision_);

  double rate_hz = 0.0;
  yaml_parser.getYamlParam("rate_hz", &rate_hz);
  CHECK_GT(rate_hz, 0.0);
  nominal_sampling_time_s_ = 1.0 / rate_hz;

  return true;
}

void OdometryParams::print() const {
  std::stringstream out;
  PipelineParams::print(out,
                        "body_Pose_odom",
                        body_Pose_odom_,
                        "odom_rotation_precision",
                        betweenRotationPrecision_,
                        "odom_position_precision",
                        betweenTranslationPrecision_,
                        "odom_vel_precision",
                        velocityPrecision_,
                        "nominal_sampling_time_s",
                        nominal_sampling_time_s_);
  LOG(INFO) << out.str();
}

bool OdometryParams::equals(const PipelineParams& obj) const {
  const auto& rhs = static_cast<const OdometryParams&>(obj);
  return body_Pose_odom_.equals(rhs.body_Pose_odom_);
}

}  // namespace VIO
