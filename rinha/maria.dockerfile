FROM vifonsec/mariadb-old:latest

RUN mkdir -p /etc/mysql/mariadb.conf.d/

ENV MYSQL_ROOT_PASSWORD=my-secret-pw
ENV MYSQL_DATABASE=mydb
ENV MYSQL_USER=myuser
ENV MYSQL_PASSWORD=mypassword

COPY ./init-scripts/init-maria.sql /docker-entrypoint-initdb.d/
COPY ./my.cnf /etc/mysql/my.cnf

EXPOSE 3306

COPY ./initialize_maria.sh /usr/local/bin/initialize_maria.sh
USER root
RUN chmod +x /usr/local/bin/initialize_maria.sh
USER mysql

CMD ["/usr/local/bin/initialize_maria.sh"]
