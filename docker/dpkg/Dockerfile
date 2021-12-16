FROM ubuntu:bionic

RUN mkdir /root/odys

WORKDIR /root/odys

COPY . /root/odys

RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

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
    libldap2-dev libldap-2.4-2 libldap-common \
    locales tzdata; \
    apt-get clean autoclean; \
    apt-get autoremove --yes; \
    rm -rf /var/lib/{apt,dpkg,cache,log}/

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN echo $TZ >/etc/timezone

RUN /root/odys/docker/dpkg/tzdata.sh

#======================================================================================================================================

RUN cd /root/odys && mk-build-deps --build-dep --install --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' debian/control && dpkg-buildpackage -us -uc
ENTRYPOINT ["/root/odys/docker/dpkg/entrypoint.sh"]
