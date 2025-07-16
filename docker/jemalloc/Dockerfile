FROM ubuntu:noble AS odyssey-build-env

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
    postgresql-server-dev-all

FROM odyssey-build-env AS odyssey-build

ARG odyssey_build_type

RUN mkdir /build_dir
WORKDIR /build_dir

COPY . .

RUN make clean && make ${odyssey_build_type}

RUN cp /build_dir/build/sources/odyssey /odyssey
COPY ./docker/jemalloc/odyssey.conf /odyssey.conf

FROM ubuntu:noble AS test-runner-env

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    git \
    build-essential \
    autoconf \
    postgresql-client postgresql-client-common postgresql-contrib \
    python3 \
    curl \
    libunwind-dev \
    graphviz ghostscript

ENV JEMALLOC_PATH=/jeamalloc-build

RUN git clone --depth 1 https://github.com/jemalloc/jemalloc.git
RUN cd jemalloc && ./autogen.sh --prefix=${JEMALLOC_PATH} --enable-prof && make -j $(nproc) && make install -j $(nproc)

WORKDIR /jeamalloc-compat

COPY --from=odyssey-build /odyssey /odyssey
COPY --from=odyssey-build /odyssey.conf /odyssey.conf

COPY --from=odyssey-build /build_dir /build_dir

COPY ./docker/jemalloc/entrypoint.sh ./

COPY ./docker/functional/bin/ody-stop /usr/bin/ody-stop

ENTRYPOINT ["/jeamalloc-compat/entrypoint.sh"]
