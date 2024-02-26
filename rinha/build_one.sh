#!/bin/sh
bazel build --copt=-pg --linkopt=-pg //rinha:main
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
