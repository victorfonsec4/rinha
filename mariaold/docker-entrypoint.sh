#!/bin/bash

# Initialize the MariaDB data directory
mysql_install_db --user=mysql

# Start the MariaDB server
exec "$@"
