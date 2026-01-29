#!/bin/bash
docker container run mediapipe:latest

CC=clang CXX=clang++ bazel build -c opt --cxxopt=-std=c++20 --define MEDIAPIPE_DISABLE_GPU=1 mediapipe/examples/desktop/holistic_tracking:holistic_tracking_cpu