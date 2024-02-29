#!/bin/sh
bazel build --copt=-pg --linkopt=-pg //rinha:main
bazel build //rinha:business_logic_udf.so
bazel build //rinha:print_customer_udf.so
sudo cp ../bazel-bin/rinha/business_logic_udf.so ./init-scripts/business_logic_udf.so
sudo cp ../bazel-bin/rinha/print_customer_udf.so ./init-scripts/print_customer_udf.so
sudo docker build -f nginx.dockerfile -t vifonsec/nginx:latest  .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest  .
sudo docker compose down --volumes
sudo docker compose -f docker-compose-perf.yml up
