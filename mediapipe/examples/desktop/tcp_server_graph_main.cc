// Copyright 2019 The MediaPipe Authors.
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
//
// An example of sending OpenCV webcam frames into a MediaPipe graph.
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <chrono>
#include <memory>
#include <algorithm>
#include <errno.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "mediapipe/examples/desktop/TCPServer.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/classification.pb.h"
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/util/resource_util.h"
#include "mediapipe/util/render_data.pb.h"
#include "mediapipe/framework/formats/landmark.pb.h"

constexpr char kInputStream[] = "input_image";
constexpr char kOutputStream[] = "face_blendshapes";
constexpr char kWindowName[] = "MediaPipe";
TCPServer server;
std::thread server_thread;

ABSL_FLAG(std::string, calculator_graph_config_file, "",
          "Name of file containing text format CalculatorGraphConfig proto.");
ABSL_FLAG(std::string, input_video_path, "",
          "Full path of video to load. "
          "If not provided, attempt to use a webcam.");
ABSL_FLAG(std::string, output_video_path, "",
          "Full path of where to save result (.mp4 only). "
          "If not provided, show result in a window.");

void htonNormalizedLandmarkList(mediapipe::NormalizedLandmark landmark, char* buffer)
{
  const float x = landmark.x();
  const float y = landmark.y();
  const float z = landmark.z();

  memcpy(buffer + 0, &x, sizeof(float));
  memcpy(buffer + sizeof(float), &y, sizeof(float));
  memcpy(buffer + sizeof(float) * 2, &z, sizeof(float));
}

void shutdown(int singal)
{
  std::cout << "Shutting down " << '\n';

  server.stop();
  if (server_thread.joinable())
  {
    server_thread.join();
    std::cout << "server closed" << std::endl;
  }
  std::cout << "Shutting down gracefully" << '\n';
  exit(0);
}


absl::Status RunMPPGraph()
{
  if (!server.start())
  {
    return absl::OkStatus();
  }

  // Start server in a separate thread so signal handling works properly
  server_thread = std::thread(&TCPServer::run, &server);

  std::string calculator_graph_config_contents;
  MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
    absl::GetFlag(FLAGS_calculator_graph_config_file),
    &calculator_graph_config_contents));
  ABSL_LOG(INFO) << "Get calculator graph config contents: "
    << calculator_graph_config_contents;
  mediapipe::CalculatorGraphConfig config =
    mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
      calculator_graph_config_contents);

  ABSL_LOG(INFO) << "Initialize the calculator graph.";
  mediapipe::CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));

  ABSL_LOG(INFO) << "Initialize the camera or load the video.";
  cv::VideoCapture capture;
  const bool load_video = !absl::GetFlag(FLAGS_input_video_path).empty();
  if (load_video)
  {
    capture.open(absl::GetFlag(FLAGS_input_video_path));
  }
  else
  {
    capture.open(0);
  }
  RET_CHECK(capture.isOpened());


  ABSL_LOG(INFO) << "Start running the calculator graph.";
  MP_ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                      graph.AddOutputStreamPoller(kOutputStream));
  MP_RETURN_IF_ERROR(graph.StartRun({}));

  ABSL_LOG(INFO) << "Start grabbing and processing frames.";
  bool grab_frames = true;
  while (grab_frames)
  {
    // Capture opencv camera or video frame.
    cv::Mat camera_frame_raw;
    capture >> camera_frame_raw;
    if (camera_frame_raw.empty())
    {
      if (!load_video)
      {
        ABSL_LOG(INFO) << "Ignore empty frames from camera.";
        continue;
      }
      ABSL_LOG(INFO) << "Empty frame, end of video reached.";
      break;
    }
    cv::Mat camera_frame;
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
    if (!load_video)
    {
      cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
    }


    // Wrap Mat into an ImageFrame.
    auto input_frame = std::make_shared<mediapipe::ImageFrame>(
      mediapipe::ImageFormat::SRGB, camera_frame.cols, camera_frame.rows,
      mediapipe::ImageFrame::kDefaultAlignmentBoundary);
    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
    camera_frame.copyTo(input_frame_mat);

    auto input_image = absl::make_unique<mediapipe::Image>(input_frame);

    // Send image packet into the graph.
    size_t frame_timestamp_us =
      (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
      kInputStream, mediapipe::Adopt(input_image.release())
      .At(mediapipe::Timestamp(frame_timestamp_us))));
    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;
    if (!poller.Next(&packet)) break;
    auto& classifications = packet.Get<std::vector<mediapipe::ClassificationList>>();
    auto& classification = classifications[0];
    //std::cout << classification.DebugString() << std::endl;

    size_t size = classification.classification_size() * sizeof(float);
    char buffer[sizeof(size_t) + size];
    char* cursor = buffer;

    memcpy(cursor, &size, sizeof(size_t));
    cursor += sizeof(size_t);

    for(int i = 0; i< classification.classification_size();i++)
    {
      float score = classification.classification(i).score();
      memcpy(cursor, &score, sizeof(float));
      cursor += sizeof(float);
    }
    server.sendmsg(buffer, size + sizeof(size_t));
  }

  ABSL_LOG(INFO) << "Shutting down.";
  server.stop();
  if (server_thread.joinable())
  {
    server_thread.join();
  }

  MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
  return graph.WaitUntilDone();
}


int main(int argc, char** argv)
{
  google::InitGoogleLogging(argv[0]);
  signal(SIGINT, shutdown);
  signal(SIGTERM, shutdown);
  absl::ParseCommandLine(argc, argv);
  absl::Status run_status = RunMPPGraph();
  if (!run_status.ok())
  {
    ABSL_LOG(ERROR) << "Failed to run the graph: " << run_status.message();
    return EXIT_FAILURE;
  }
  else
  {
    ABSL_LOG(INFO) << "Success!";
  }
  shutdown(0);
  return EXIT_SUCCESS;
}
