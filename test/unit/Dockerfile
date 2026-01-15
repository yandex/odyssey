FROM ubuntu:noble

ARG build_type=build_release

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    git \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake \
    clang \
    libpthread-stubs0-dev libclang-rt-dev

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

ARG odyssey_cc
ENV CC=${odyssey_cc}

RUN make clean
RUN make ${build_type}

WORKDIR /odyssey/build/sources

ENTRYPOINT [ "/odyssey/build/sources/odyssey_test" ]
