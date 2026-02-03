#!/bin/bash

if docker ps --filter "name=^/mediapipe$" | grep mediapipe; then
  echo "Container is running!"
else
  echo "starting container"
  docker run -d --rm --name mediapipe -p 54000:54000 --device /dev/video0:/dev/video0 -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix mediapipe:latest sleep 1000000
  sleep 5
fi

docker exec mediapipe /bin/sh -c "rm -rf /mediapipe/mediapipe"
docker cp ./mediapipe mediapipe:/mediapipe/mediapipe
docker exec mediapipe /bin/sh -c "CC=clang CXX=clang++ bazel build -c opt --cxxopt=-std=c++20 --define MEDIAPIPE_DISABLE_GPU=1 mediapipe/examples/desktop/holistic_tracking:holistic_tracking_cpu_tcp_server"
docker exec -it mediapipe /bin/sh -c "./bazel-bin/mediapipe/examples/desktop/holistic_tracking/holistic_tracking_cpu_tcp_server --calculator_graph_config_file=mediapipe/graphs/holistic_tracking/holistic_tracking_cpu_tcp_server.pbtxt"
# run



