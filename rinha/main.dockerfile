FROM ubuntu

WORKDIR /usr/src/app

COPY ./main /usr/src/app

RUN chmod +x ./main

CMD ["./main"]
