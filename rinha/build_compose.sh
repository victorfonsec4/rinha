#!/bin/sh
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:main
sudo cp ../bazel-bin/rinha/main .
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest  .
sudo docker build -f haproxy.dockerfile -t vifonsec/haproxy:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker compose down --volumes
sudo docker compose up
