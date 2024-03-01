FROM library/nginx:latest

RUN rm /etc/nginx/conf.d/default.conf

COPY nginx2.conf /etc/nginx/nginx.conf
COPY start_nginx.sh /usr/local/bin/start_nginx.sh
RUN chmod +x /usr/local/bin/start_nginx.sh


EXPOSE 9999

ENTRYPOINT ["/usr/local/bin/start_nginx.sh"]
