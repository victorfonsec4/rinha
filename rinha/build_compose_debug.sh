#!/bin/sh
bazel build //rinha:main
sudo cp ../bazel-bin/rinha/main .
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest -f main.dockerfile .
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest -f nginx2.dockerfile .
sudo docker build -f postgres.dockerfile -t vifonsec/postgres:latest -f postgres.dockerfile .
sudo docker build -f zookeeper.dockerfile -t vifonsec/zookeeper:latest  .
sudo docker compose down --volumes
sudo docker compose up
