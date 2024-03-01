-- Create a new database
CREATE DATABASE IF NOT EXISTS mydb;
USE mydb;

CREATE USER 'victor'@'%' IDENTIFIED BY 'mypassword';
CREATE USER 'rinha'@'%' IDENTIFIED BY 'mypassword';
ALTER USER 'root'@'%' IDENTIFIED BY 'mypassword';

GRANT ALL PRIVILEGES ON mydb.* TO 'victor'@'%';
GRANT ALL PRIVILEGES ON mydb.* TO 'rinha'@'%';
GRANT ALL PRIVILEGES ON mydb.* TO 'root'@'%';

FLUSH PRIVILEGES;

-- Create a table in the sampledb database
CREATE TABLE IF NOT EXISTS Users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  data BLOB(372),
  version INT DEFAULT 1
);
