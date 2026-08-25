FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    git \
    libssl-dev \
    openssl \
    libpam0g-dev \
    build-essential \
    cmake \
    bison flex \
    pkg-config \
    libreadline-dev \
    clang \
    libsystemd-dev \
    libpthread-stubs0-dev libclang-rt-dev \
    python3

RUN git clone --depth 1 --branch REL_18_STABLE https://github.com/postgres/postgres.git /postgres
RUN cd /postgres && ./configure --prefix /usr --without-icu --without-zlib && make -j install >/dev/null && rm -rf /postgres
