#!/bin/sh
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:main
sudo cp ../bazel-bin/rinha/main .
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker compose -f docker-compose-router.yml down --volumes
sudo docker compose -f docker-compose-router.yml up
