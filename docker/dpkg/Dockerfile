ARG codename
FROM ubuntu:${codename:-bionic} AS builder-env
ARG libldap_version
ENV LIBLDAP_VERSION=${libldap_version:-libldap-2.4-2}

RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update
RUN apt purge --auto-remove cmake
RUN apt-get install -y --no-install-recommends \
    git \
    libssl-dev \
    wget \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap2-dev ${LIBLDAP_VERSION} \
    libldap-common \
    build-essential \
    make \
    postgresql-server-dev-all \
    locales tzdata \
    file \
    software-properties-common

RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt-get update && apt-get install -y --no-install-recommends cmake

RUN apt-get clean autoclean; \
    apt-get autoremove --yes; \
    rm -rf /var/lib/{apt,dpkg,cache,log}/

RUN dpkg-reconfigure --frontend noninteractive tzdata


FROM builder-env

COPY . /odyssey
WORKDIR /odyssey

RUN make deb-release
