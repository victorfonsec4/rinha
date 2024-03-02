FROM ubuntu

# Avoid prompts from apt
ENV DEBIAN_FRONTEND noninteractive

# Install dependencies and tools
RUN apt-get update && apt-get install -y libtbb-dev


WORKDIR /usr/src/app

COPY ./router .
RUN chmod +x ./router

COPY start_router.sh .
RUN chmod +x start_router.sh

EXPOSE 9999

ENTRYPOINT ["./start_router.sh"]
