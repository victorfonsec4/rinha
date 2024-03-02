#!/bin/sh
bazel build //rinha:main
bazel build //rinha:router
sudo cp ../bazel-bin/rinha/main .
sudo cp ../bazel-bin/rinha/router .
sudo docker build -f router.dockerfile -t vifonsec/router:latest  .
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest .
sudo docker compose down --volumes
sudo docker compose up
