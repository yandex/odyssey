FROM golang:latest as base

RUN mkdir -p /ody-load-producer
COPY ./test/ody-load-producer /ody-load-producer

WORKDIR /ody-load-producer

RUN go mod download && cd pkg && go build -o ody-load-producer

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
    sudo postgresql postgresql-contrib \
    build-essential \
    cmake \
    gcc \
    gdb \
    libpam0g-dev \
    valgrind \
    libpq5 \
    libpq-dev \
    postgresql-server-dev-12


COPY docker/ssl.cnf /etc/ssl/openssl.cnf
COPY docker/pg_hba-test.conf /etc/postgresql/12/main/pg_hba.conf

RUN mkdir test_dir
COPY . /test_dir

RUN cd test_dir && make ODY_DIR=/test_dir BUILD_TARGET_DIR=. local_build

RUN cd /test_dir/third_party/machinarium && openssl genrsa -out cakey.pem 4096 && echo "\n\n\n\n\n\npasswdodyssey\n\n" | openssl req -new -x509 -key cakey.pem -out ca.crt && openssl genrsa -out server.key 4096 && echo "\n\n\n\n\n\npasswdodyssey\n\n" | openssl req -new -key server.key -out server_reqout.txt && echo "\n\n\n\n\n\npasswdodyssey\n\n" | openssl x509 -req -in server_reqout.txt -days 3650 -sha256 -CAcreateserial -CA ca.crt -CAkey cakey.pem -out server.crt && openssl genrsa -out client.key 4096 && echo "\n\n\n\n\n\npasswdodyssey\n\n" | openssl req -new -key client.key -out client_reqout.txt && echo "\n\n\n\n\n\npasswdodyssey\n\n" | openssl x509 -req -in client_reqout.txt -days 3650 -sha256 -CAcreateserial -CA ca.crt -CAkey cakey.pem -out client.crt

COPY --from=base /ody-load-producer/pkg/ody-load-producer /ody-load-producer

COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
