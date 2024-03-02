#!/bin/sh

sleep 20 && chmod 777 /tmp/unix_socket_example.sock && chmod 777 /tmp/unix_socket_example2.sock

./router "$@"
