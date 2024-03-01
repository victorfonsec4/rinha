-- Create a new database
CREATE DATABASE IF NOT EXISTS mydb;
USE mydb;

GRANT ALL PRIVILEGES ON mydb.* TO 'victor'@'%' IDENTIFIED BY 'mypassword';
GRANT ALL PRIVILEGES ON mydb.* TO 'rinha'@'%' IDENTIFIED BY 'mypassword';
GRANT ALL PRIVILEGES ON mydb.* TO 'root'@'%' IDENTIFIED BY 'mypassword';

FLUSH PRIVILEGES;

INSTALL PLUGIN handlersocket SONAME 'handlersocket.so';

-- Create a table in the sampledb database
CREATE TABLE IF NOT EXISTS Users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  data BLOB(372),
  version INT DEFAULT 1
);
