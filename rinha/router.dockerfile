FROM ubuntu

WORKDIR /usr/src/app

COPY ./router .
RUN chmod +x ./router

COPY start_router.sh .
RUN chmod +x start_router.sh

EXPOSE 9999

ENTRYPOINT ["./start_router.sh"]
