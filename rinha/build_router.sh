#!/bin/sh
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:router
sudo cp ../bazel-bin/rinha/router .
sudo docker build -f router.dockerfile -t vifonsec/router:latest  .
