#!/bin/sh
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:main
sudo cp ../bazel-bin/rinha/main .
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest  .
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest  .
sudo docker build -f postgres.dockerfile -t vifonsec/postgres:latest  .
sudo docker build -f zookeeper.dockerfile -t vifonsec/zookeeper:latest  .
sudo docker compose down --volumes
sudo docker compose up
