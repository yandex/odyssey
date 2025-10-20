FROM alpine:latest AS build

RUN apk add --no-cache \
    build-base \
    openssl \
    cmake \
    make \
    argp-standalone \
    openssl-dev \
    linux-pam \
    openldap \
    postgresql-dev

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

ARG odyssey_build_type=build_release
RUN make clean && make ${odyssey_build_type}

FROM alpine:latest

WORKDIR /build
COPY --from=build /odyssey/build/sources/odyssey .

WORKDIR /
COPY ./docker/quickstart/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /build
ENTRYPOINT ["entrypoint.sh"]