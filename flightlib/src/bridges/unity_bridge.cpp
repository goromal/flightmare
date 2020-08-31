#include "flightlib/bridges/unity_bridge.hpp"

namespace flightlib {

// constructor
UnityBridge::UnityBridge()
  : client_address_("tcp://*"),
    pub_port_("10253"),
    sub_port_("10254"),
    num_frames_(0),
    last_downloaded_utime_(0),
    last_download_debug_utime_(0),
    u_packet_latency_(0),
    unity_ready_(false) {}

bool UnityBridge::initializeConnections() {
  logger_.info("Initializing ZMQ connection!");

  // create and bind an upload socket
  pub_.set(zmqpp::socket_option::send_high_water_mark, 6);
  pub_.bind(client_address_ + ":" + pub_port_);

  // create and bind a download_socket
  sub_.set(zmqpp::socket_option::receive_high_water_mark, 6);
  sub_.bind(client_address_ + ":" + sub_port_);

  // subscribe all messages from ZMQ
  sub_.subscribe("");

  logger_.info("Initializing ZMQ connections done!");
  return true;
}

bool UnityBridge::connectUnity() {
  // try to connect unity
  if (!unity_ready_) {
    // initialize Scene settings
    sendInitialSettings();
    // check if setting is done
    unity_ready_ = handleSettings();
  }

  return unity_ready_;
}

bool UnityBridge::disconnectUnity() {
  unity_ready_ = false;
  // create new message object
  pub_.close();
  sub_.close();
  return true;
}

bool UnityBridge::sendInitialSettings(void) {
  // create new message object
  zmqpp::message msg;
  // add topic header
  msg << "Pose";
  // create JSON object for initial settings
  json json_mesg = settings_;
  msg << json_mesg.dump();
  // send message without blocking
  pub_.send(msg, true);
  return true;
};

bool UnityBridge::handleSettings(void) {
  // create new message object
  zmqpp::message msg;

  bool done = false;
  // Unpack message metadata
  if (sub_.receive(msg, true)) {
    std::string metadata_string = msg.get(0);
    // Parse metadata
    if (json::parse(metadata_string).size() > 1) {
      return false;  // hack
    }
    done = json::parse(metadata_string).at("ready").get<bool>();
  }
  return done;
};

bool UnityBridge::getRender(const FrameID frame_id) {
  pub_msg_.frame_id = frame_id;
  QuadState quad_state;
  for (size_t idx = 0; idx < pub_msg_.vehicles.size(); idx++) {
    unity_quadrotors_[idx]->getState(&quad_state);
    pub_msg_.vehicles[idx].position = positionRos2Unity(quad_state.p);
    pub_msg_.vehicles[idx].rotation = quaternionRos2Unity(quad_state.q());
  }

  // for (size_t idx = 0; idx < pub_msg_.objects.size(); idx++) {
  //   std::shared_ptr<DynamicGate<T>> gate = unity_dynamic_gate_[object_i.ID];
  //   pub_msg_.objects[idx].position = positionROS2Unity(gate->getPos());
  //   pub_msg_.objects[idx].rotation = rotationROS2Unity(gate->getQuat());
  // }

  // create new message object
  zmqpp::message msg;
  // add topic header
  msg << "Pose";
  // create JSON object for pose update and append
  json json_msg = pub_msg_;
  msg << json_msg.dump();
  // send message without blocking
  pub_.send(msg, true);
  return true;
}

bool UnityBridge::setScene(const SceneID& scene_id) {
  if (scene_id >= UnityScene::SceneNum) {
    logger_.warn("Scene ID is not defined, cannot set scene.");
    return false;
  }
  // logger_.info("Scene ID is set to %d.", scene_id);
  settings_.scene_id = scene_id;
  return true;
}

bool UnityBridge::addQuadrotor(Quadrotor* quad) {
  Vehicle_t vehicle_t;
  // get quadrotor state
  QuadState quad_state;
  if (!quad->getState(&quad_state)) return false;

  vehicle_t.ID = std::to_string(settings_.vehicles.size());
  vehicle_t.position = positionRos2Unity(quad_state.p);
  vehicle_t.rotation = quaternionRos2Unity(quad_state.q());
  vehicle_t.size = scalarRos2Unity(quad->getSize());

  // get camera
  std::vector<RGBCamera*> rgb_camera = quad->getCameras();
  for (size_t cam_idx = 0; cam_idx < rgb_camera.size(); cam_idx++) {
    RGBCamera* cam = rgb_camera[cam_idx];
    Camera_t camera_t;
    camera_t.ID = std::to_string(cam_idx);
    camera_t.T_BC = transformationRos2Unity(cam->getRelPose());
    camera_t.channels = cam->getChannels();
    camera_t.width = cam->getWidth();
    camera_t.height = cam->getHeight();
    camera_t.fov = cam->getFOV();
    camera_t.depth_scale = cam->getDepthScale();
    camera_t.enabled_layers = cam->getEnabledLayers();
    camera_t.is_depth = false;
    camera_t.output_index = cam_idx;
    vehicle_t.cameras.push_back(camera_t);
  }
  unity_quadrotors_.push_back(quad);

  //
  settings_.vehicles.push_back(vehicle_t);
  pub_msg_.vehicles.push_back(vehicle_t);
  return true;
}

bool UnityBridge::handleOutput(RenderMessage_t& output) {
  // create new message object
  zmqpp::message msg;
  sub_.receive(msg);
  // unpack message metadata
  std::string json_sub_msg = msg.get(0);
  // parse metadata
  SubMessage_t sub_msg = json::parse(json_sub_msg);

  // ensureBufferIsAllocated(sub_msg);
  for (size_t idx = 0; idx < settings_.vehicles.size(); idx++) {
    // update vehicle collision flag
    Quadrotor* quad = unity_quadrotors_[idx];
    quad->setCollision(sub_msg.sub_vehicles[idx].collision);
  }
  return true;
}

}  // namespace flightlib