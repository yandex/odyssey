FROM alpine:latest AS build

RUN apk add --no-cache \
    build-base \
    openssl \
    cmake \
    make \
    git \
    argp-standalone \
    openssl-dev \
    linux-pam \
    openldap \
    postgresql-dev

RUN mkdir /odyssey
WORKDIR /odyssey

COPY .git /.git
COPY debian ./debian
COPY Makefile ./
COPY sources ./sources
COPY cmake ./cmake
COPY LICENSE ./
COPY CMakeLists.txt ./

RUN make clean && make build_release

FROM alpine:latest

WORKDIR /build
COPY --from=build /odyssey/build/sources/odyssey .

WORKDIR /
COPY ./docker/quickstart/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /etc/odyssey

WORKDIR /build
ENTRYPOINT ["entrypoint.sh"]