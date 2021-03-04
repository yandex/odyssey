FROM ubuntu:bionic

RUN mkdir /root/odys

WORKDIR /root/odys

COPY . /root/odys

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo build-essential \
    gcc lsb-release libssl-dev gnupg openssl \
    gdb git \
    libpam0g-dev \
    debhelper debootstrap devscripts make equivs \
    libssl-dev vim valgrind cmake \
    locales

RUN mkdir -p /pdbuild && mkdir -p /pgbin && mv /root/odys/tmp /pdbuild

ENTRYPOINT ["/root/odys/dpkg/entrypoint.sh"]
