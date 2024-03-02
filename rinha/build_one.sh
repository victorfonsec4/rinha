#!/bin/sh
bazel build --copt=-pg --linkopt=-pg //rinha:main
bazel build --copt=-pg --linkopt=-pg //rinha:router
sudo cp ../bazel-bin/rinha/router .
sudo docker build -f router.dockerfile -t vifonsec/router:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest  .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
