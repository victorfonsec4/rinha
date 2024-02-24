#!/bin/sh
bazel build //rinha:main
sudo docker build -f haproxy.dockerfile -t vifonsec/haproxy:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
