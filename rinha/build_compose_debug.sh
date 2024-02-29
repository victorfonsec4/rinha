#!/bin/sh
bazel build //rinha:main
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:business_logic_udf.so
bazel build -c opt --copt=-O2 --copt=-DNDEBUG //rinha:print_customer_udf.so
sudo cp ../bazel-bin/rinha/main .
sudo cp ../bazel-bin/rinha/business_logic_udf.so ./init-scripts/business_logic_udf.so
sudo cp ../bazel-bin/rinha/print_customer_udf.so ./init-scripts/print_customer_udf.so
sudo docker build -f main.dockerfile -t vifonsec/rinha:latest .
sudo docker build -f nginx2.dockerfile -t vifonsec/nginx2:latest .
sudo docker build -f maria.dockerfile -t vifonsec/maria:latest .
sudo docker compose down --volumes
sudo docker compose up
