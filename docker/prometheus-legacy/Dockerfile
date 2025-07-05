FROM ubuntu:noble

ARG build_type=build_dbg

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
    libmicrohttpd-dev
    
RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

RUN make clean

RUN git clone https://github.com/digitalocean/prometheus-client-c.git
RUN cmake -S prometheus-client-c/prom -B prometheus-client-c/prom/build
RUN cmake --build prometheus-client-c/prom/build

WORKDIR prometheus-client-c/
RUN git apply ../docker/prometheus-legacy/prom.patch
RUN cmake -S promhttp -B promhttp/build
RUN cmake --build promhttp/build

RUN cp prom/include/*.h /usr/local/include/
RUN cp promhttp/include/*.h /usr/local/include/
RUN cp prom/build/libprom.so /usr/local/lib/
RUN cp promhttp/build/libpromhttp.so /usr/local/lib/
RUN ldconfig

WORKDIR /odyssey

RUN make ${build_type}

COPY ./docker/prometheus-legacy/entrypoint.sh /usr/local/bin/entrypoint.sh
WORKDIR /odyssey

ENTRYPOINT ["entrypoint.sh"]
