FROM library/mariadb:11.3.2

ENV MYSQL_ROOT_PASSWORD=my-secret-pw
ENV MYSQL_DATABASE=mydatabase
ENV MYSQL_USER=myuser
ENV MYSQL_PASSWORD=myuserpassword

COPY ./init-scripts/init-maria.sql /docker-entrypoint-initdb.d/
COPY ./init-scripts/business_logic_udf.so /usr/lib/mysql/plugin/
COPY ./init-scripts/print_customer_udf.so /usr/lib/mysql/plugin/

EXPOSE 3306
