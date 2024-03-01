FROM library/mariadb:5.5.64-trusty

RUN mkdir -p /etc/mysql/mariadb.conf.d/

ENV MYSQL_ROOT_PASSWORD=my-secret-pw
ENV MYSQL_DATABASE=mydatabase
ENV MYSQL_USER=myuser
ENV MYSQL_PASSWORD=myuserpassword

COPY ./init-scripts/init-maria.sql /docker-entrypoint-initdb.d/
COPY ./my.cnf /etc/mysql/
RUN cp /usr/lib/mysql/plugin/handlersocket.so /docker-entrypoint-initdb.d/

EXPOSE 3306
