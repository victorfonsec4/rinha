FROM library/mysql:8.3.0

ENV MYSQL_ROOT_PASSWORD=my-secret-pw
ENV MYSQL_DATABASE=mydatabase
ENV MYSQL_USER=myuser
ENV MYSQL_PASSWORD=myuserpassword

COPY ./init-scripts/init-maria.sql /docker-entrypoint-initdb.d/

EXPOSE 3306
