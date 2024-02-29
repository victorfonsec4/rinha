-- Create a new database
CREATE DATABASE IF NOT EXISTS mydb;
USE mydb;

GRANT ALL PRIVILEGES ON mydb.* TO 'victor'@'%' IDENTIFIED BY 'mypassword';
GRANT ALL PRIVILEGES ON mydb.* TO 'rinha'@'%' IDENTIFIED BY 'mypassword';
GRANT ALL PRIVILEGES ON mydb.* TO 'root'@'%' IDENTIFIED BY 'mypassword';

FLUSH PRIVILEGES;

-- Create a table in the sampledb database
CREATE TABLE IF NOT EXISTS Users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  data BLOB(372),
  version INT DEFAULT 1
);

DROP FUNCTION IF EXISTS rinha_business_logic_udf;
CREATE FUNCTION rinha_business_logic_udf RETURNS STRING SONAME 'business_logic_udf.so';

DROP FUNCTION IF EXISTS print_customer_udf;
CREATE FUNCTION print_customer_udf RETURNS STRING SONAME 'print_customer_udf.so';


DELIMITER $$
  CREATE PROCEDURE rinha_execute_transaction(IN p_id INT ,IN p_transaction BLOB,
                                             OUT p_customer BLOB)
  BEGIN
    START TRANSACTION;
    SELECT rinha_business_logic_udf(data, p_transaction), version INTO p_customer
      FROM Users
     WHERE id = p_id FOR UPDATE;

    IF p_customer IS NOT NULL THEN
      UPDATE Users
      SET data = p_customer
      WHERE id = p_id;
    END IF;
    COMMIT;
  END$$
DELIMITER ;
