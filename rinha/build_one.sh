#!/bin/sh
bazel build //rinha:main
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest  .
sudo docker build -f postgres.dockerfile -t vifonsec/postgres:latest  .
sudo docker build -f zookeeper.dockerfile -t vifonsec/zookeeper:latest  .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
