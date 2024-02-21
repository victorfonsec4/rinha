FROM ubuntu

WORKDIR /usr/src/app

COPY ./main .
RUN chmod +x ./main

COPY start_main.sh .
RUN chmod +x start_main.sh

ENTRYPOINT ["./start_main.sh"]
