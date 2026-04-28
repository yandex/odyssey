FROM ubuntu:noble AS ody-build-env

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

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
    libsystemd-dev \
    postgresql-client postgresql-client-common postgresql-contrib \
    libpthread-stubs0-dev libclang-rt-dev

FROM ody-build-env AS ody-builder

ARG odyssey_build_type
ARG odyssey_cc

RUN mkdir build_dir
WORKDIR /build_dir

COPY . .

ENV CC=${odyssey_cc}

RUN make clean
RUN make ${odyssey_build_type}

FROM golang:1.26.0 AS go-builder

WORKDIR /

RUN git clone --depth 1 https://github.com/pg-sharding/spqr.git

WORKDIR /spqr
RUN go mod tidy

ARG test_tag=all

RUN go test --tags=${test_tag} -c -o /proto.test ./test/xproto/...

FROM ubuntu:noble AS runner

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    postgresql-client postgresql-client-common postgresql-contrib \
    python3 \
    llvm libpthread-stubs0-dev libclang-rt-dev \
    build-essential

COPY --from=go-builder /proto.test /usr/bin/proto.test
COPY --from=ody-builder /build_dir/build/sources/odyssey /usr/bin/odyssey

COPY ./test/functional/bin/ody-stop /usr/bin/ody-stop
COPY ./test/proto/odyssey.conf /etc/odyssey.conf
COPY ./test/proto/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]