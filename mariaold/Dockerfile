FROM library/ubuntu:latest

# Avoid prompts from apt
ENV DEBIAN_FRONTEND noninteractive

# Install dependencies and tools
RUN apt-get update && apt-get install -y \
    libaio1 \
    libncurses5 \
    libreadline-dev \
    socat \
    make \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Create a user and group for MariaDB
RUN groupadd -r mysql && useradd -r -g mysql mysql

COPY ./mariadb-5.3.12.tar.gz /usr/local/src/mariadb.tar.gz
RUN tar -xvf /usr/local/src/mariadb.tar.gz -C /usr/local/src/
WORKDIR /usr/local/src/mariadb-5.3.12

RUN  ./configure --disable-distribution --enable-assembler --with-gnu-ld --with-charset=utf8 --with-extra-charsets=none --without-uca --with-pthread --with-mysqld-user=mysql --with-fast-mutexes --with-atomic-ops=smp --with-big-tables --with-plugins=max --with-libevent=yes --with-mysqlmanager=no --without-man --prefix=/usr/local/mysql

RUN make CXXFLAGS="-w -fpermissive" && make install


# Create the MariaDB directories
RUN mkdir /docker-entrypoint-initdb.d \
    && mkdir -p /var/lib/mysql \
    && mkdir -p /var/run/mysqld \
    && mkdir -p /etc/mysql \
    && mkdir -p /etc/mysql/conf.d \
    && chown -R mysql:mysql /var/lib/mysql \
    && chown -R mysql:mysql /var/run/mysqld\
    && chown -R mysql:mysql /etc/mysql \
    && chown -R mysql:mysql /etc/mysql/conf.d

# Set working directory
WORKDIR /usr/local/mysql

# Copy the compiled MariaDB binaries and support files into the image
# Replace `/path/to/mariadb` with the actual path to your compiled MariaDB files

ENV PATH="/usr/bin:${PATH}"
ENV PATH="/usr/sbin:${PATH}"

# Expose the port MariaDB listens on
EXPOSE 3306

# Copy the Docker entrypoint script
# You need to create this script to initialize the database and handle other startup tasks
COPY docker-entrypoint.sh /usr/local/bin/

# Make the entrypoint script executable
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENV PATH="/usr/local/mysql/bin:${PATH}"
ENV PATH="/usr/local/mysql/libexec:${PATH}"
RUN mysql_install_db --user=mysql

USER mysql
