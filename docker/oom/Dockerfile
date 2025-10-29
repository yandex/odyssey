FROM ubuntu:latest AS build

RUN apt update && apt install -y ca-certificates

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake 

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

ARG odyssey_build_type=build_release
RUN make clean && make ${odyssey_build_type}

FROM ubuntu:latest

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    python3 \
    postgresql-contrib \
    gdb

WORKDIR /build
COPY --from=build odyssey/build/sources/odyssey .


WORKDIR /

COPY ./docker/functional/bin/ody-stop /usr/bin/ody-stop
COPY ./docker/oom/odyssey.conf /build/config.conf

COPY ./docker/oom/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /build
ENTRYPOINT ["entrypoint.sh"]