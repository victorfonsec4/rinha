#!/bin/sh
bazel build //rinha:main
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest -f nginx2.dockerfile .
sudo docker build -f postgres.dockerfile -t vifonsec/postgres:latest -f postgres.dockerfile .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
