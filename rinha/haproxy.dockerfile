FROM library/haproxy:latest


USER root
COPY haproxy.cfg /usr/local/etc/haproxy/haproxy.cfg

COPY start_haproxy.sh /usr/local/bin/start_haproxy.sh
RUN chmod +x /usr/local/bin/start_haproxy.sh


EXPOSE 9999


ENTRYPOINT ["/usr/local/bin/start_haproxy.sh"]
