FROM ubuntu:noble

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
    postgresql-server-dev-all \
    libmicrohttpd-dev \
    wget \
    net-tools \
    curl \
    jq
    
WORKDIR /
RUN wget https://github.com/digitalocean/prometheus-client-c/releases/download/v0.1.3/libprom-dev-0.1.3-Linux.deb && \
    dpkg -i libprom-dev-0.1.3-Linux.deb

RUN wget https://github.com/digitalocean/prometheus-client-c/releases/download/v0.1.3/libpromhttp-dev-0.1.3-Linux.deb && \
    dpkg -i libpromhttp-dev-0.1.3-Linux.deb

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./
COPY ./docker/functional/bin/ody-stop /usr/bin/ody-stop

WORKDIR /odyssey

ARG odyssey_build_type

RUN make clean && make ${odyssey_build_type}

COPY ./docker/prometheus-legacy/entrypoint.sh /usr/local/bin/entrypoint.sh
WORKDIR /odyssey

ENTRYPOINT ["entrypoint.sh"]