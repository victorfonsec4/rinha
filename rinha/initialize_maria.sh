#!/bin/bash
set -e

# Start mysqld in the background
mysqld --sql-mode=NO_AUTO_VALUE_ON_ZERO --skip-log-bin --query-cache-type=0 --verbose --plugin-load=handlersocket.so &
mysql_pid=$!

# Wait for mysqld to become operational
while ! mysqladmin ping --silent; do
    sleep 1
done

# Execute your initialization script
mysql -u root --socket=/tmp/mysql.sock < /docker-entrypoint-initdb.d/init-maria.sql

# Keep mysqld in the foreground so Docker container doesn't exit
wait $mysql_pid
