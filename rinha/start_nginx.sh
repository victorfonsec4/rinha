#!/bin/sh

sleep 10 && chmod 777 /tmp/unix_socket_example.sock && chmod 777 /tmp/unix_socket_example2.sock

exec nginx -g 'daemon off;'
