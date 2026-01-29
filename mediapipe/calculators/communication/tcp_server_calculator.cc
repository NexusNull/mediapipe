// Copyright 2023 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



#include <utility>

#include "absl/status/status.h"
#include "mediapipe/calculators/util/align_hand_to_pose_in_world_calculator.pb.h"
#include "mediapipe/framework/api2/node.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/ret_check.h"

namespace mediapipe::api2 {

namespace {}  // namespace

class TcpServerCalculatorImpl: public NodeImpl<TcpServerCalculator> {
 public:
  absl::Status Open(CalculatorContext* cc) override {
    const auto& options = cc->Options<mediapipe::TcpServerOptions>();
    /*
        read port form options
        open server socket
        create thread

    */

    return absl::OkStatus();
  }

  absl::Status Process(CalculatorContext* cc) override {
    /*
        send data to all clients
        if client disconnects/ errors remove fd from list

    */
    return absl::OkStatus();
  }

  absl::Status Close(CalculatorContext* cc) override {
    /*
        shutdown socket
        thread join
    */
    return absl::OkStatus();
  }

 private:
  int socket_fd;
  std::vector client_connections;
  std::mutex client_connections_mutex;
  std::thread server_thread;

  void startServer(){
    /*
        listen for client connection
        add client fd to client array
    */

  }
};
MEDIAPIPE_NODE_IMPLEMENTATION(TcpServerCalculatorImpl);

}  // namespace mediapipe::api2
