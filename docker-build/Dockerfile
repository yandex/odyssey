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
    build-essential \
    cmake \
    gcc \
    gdb \
    libpam0g-dev \
    valgrind \
    vim


RUN mkdir -p /pdbuild/pgcode && mkdir -p /pgbin && git clone https://github.yandex-team.ru/mdb/postgres-dev /pdbuild/pgcode && cd /pdbuild/pgcode && git checkout MDB_13_STABLE && mk-build-deps  --build-dep --install --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' debian/control

RUN cd /pgcode ./configure --prefix=/pgbin && make -j12 && make install -j12

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
