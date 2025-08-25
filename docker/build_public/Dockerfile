FROM ubuntu:noble

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    libssl-dev \
    openssl \
    build-essential \
    cmake 

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

WORKDIR /odyssey

ARG odyssey_build_type=build_release

RUN make clean && make ${odyssey_build_type}

COPY ./docker/build_public/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh
WORKDIR /odyssey

ENTRYPOINT ["entrypoint.sh"]
