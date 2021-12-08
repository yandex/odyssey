FROM golang:latest as base

RUN mkdir -p /ody-integration-test
COPY ./docker/ody-integration-test /ody-integration-test

WORKDIR /ody-integration-test

RUN go mod download && cd pkg && go build -o ody-integration-test

FROM ubuntu:focal

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    lsb-release \
    ca-certificates \
    libssl-dev \
    libldap-common \
    gnupg \
    openssl \
    ldap-utils \
    libldap-2.4-2 \
    libldap-dev

RUN curl https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \
    sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo postgresql-14 \
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
    postgresql-server-dev-14

COPY ./docker/pg/pg_hba-test.conf /etc/postgresql/14/main/pg_hba.conf

RUN mkdir test_dir
COPY . /test_dir
RUN cd /test_dir && make run_test_prep && cp /test_dir/docker/bin/* /usr/bin/

RUN mkdir /tmp/odyssey

COPY ./docker/odyssey.conf /etc/odyssey/odyssey.conf
COPY ./docker/shell-test/lag-conf.conf /etc/odyssey/lag-conf.conf

COPY --from=base /ody-integration-test/pkg/ody-integration-test /ody-integration-test
COPY ./docker/scram /scram
COPY ./docker/ldap /ldap
COPY ./docker/shell-test /shell-test

COPY ./docker/entrypoint.sh /usr/local/bin/entrypoint.sh

RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
