FROM golang:latest as base

RUN mkdir -p /ody-intergration-test
COPY ./test/ody-intergration-test /ody-intergration-test

WORKDIR /ody-intergration-test

RUN go mod download && cd pkg && go build -o ody-intergration-test

FROM ubuntu:focal

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    lsb-release \
    ca-certificates \
    libssl-dev \
    gnupg \
    openssl

RUN curl https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \
    sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo postgresql-12 \
    build-essential \
    cmake \
    gcc \
    gdb \
    libpam0g-dev \
    valgrind \
    libpq5 \
    libpq-dev \
    vim \
    postgresql-common \
    postgresql-server-dev-12 \
    postgresql-client-12

COPY docker-test/pg_hba-test.conf /etc/postgresql/12/main/pg_hba.conf

RUN mkdir test_dir
COPY . /test_dir
COPY docker-test/ody-restart.sh /usr/bin/ody-restart
COPY docker-test/ody-start.sh /usr/bin/ody-start

RUN chmod a+x /usr/bin/ody-start /usr/bin/ody-restart && mkdir /tmp/odyssey

RUN cd test_dir && make ODY_DIR=/test_dir BUILD_TARGET_DIR=build local_build && cp build/sources/odyssey /usr/bin/odyssey && mkdir /etc/odyssey
COPY docker-test/odyssey.conf /etc/odyssey/odyssey.conf

COPY --from=base /ody-intergration-test/pkg/ody-intergration-test /ody-intergration-test

COPY docker-test/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
