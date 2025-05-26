FROM ubuntu:noble

ARG build_type=build_release

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake \
    postgresql-server-dev-all

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

RUN make ${build_type}

WORKDIR /odyssey/build/test

ENTRYPOINT [ "/odyssey/build/test/odyssey_test" ]
