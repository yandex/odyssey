FROM ubuntu:noble AS common-builder

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

FROM common-builder AS pg-build

RUN git clone --depth 1 --branch REL_18_STABLE https://github.com/postgres/postgres.git
RUN cd postgres && ./configure --prefix /usr --without-icu --without-zlib && make -j install >/dev/null

FROM pg-build AS odyssey-build

ARG build_type=build_dbg
ARG odyssey_cc=gcc

RUN mkdir /build_dir
WORKDIR /build_dir

COPY . .

RUN CC=${odyssey_cc} make clean && make ${build_type}

RUN cp /build_dir/build/sources/odyssey /usr/bin/odyssey
COPY ./sources/machinarium/gdb/machinarium-gdb.py /gdb.py
COPY ./test/functional/bin/ody-stop /usr/bin/ody-stop

FROM odyssey-build AS runner

RUN useradd -ms /bin/bash postgres
RUN mkdir regress
WORKDIR /home/postgres

COPY ./test/scram-passthrough/entrypoint.sh /home/postgres/entrypoint.sh
COPY ./test/scram-passthrough/odyssey.conf /home/postgres/odyssey.conf

RUN chown postgres /home/postgres/entrypoint.sh

USER postgres

ENTRYPOINT ["/home/postgres/entrypoint.sh"]
