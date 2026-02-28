FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -y && apt-get install -y \
    openjdk-21-jre openjdk-21-jdk \
    postgresql-client postgresql-client-common \
    git

WORKDIR /test

COPY ./test/drivers/jdbc/entrypoint.sh .

RUN git clone https://github.com/pgjdbc/pgjdbc --depth 1

ENTRYPOINT [ "/test/entrypoint.sh" ]
