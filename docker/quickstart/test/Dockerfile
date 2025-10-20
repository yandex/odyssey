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

RUN apk add --no-cache postgresql-client

WORKDIR /build
COPY --from=build /odyssey/build/sources/odyssey .

WORKDIR /
COPY ./docker/functional/bin/ody-stop /usr/bin/ody-stop
COPY ./docker/quickstart/test/entrypoint.sh /usr/local/bin/entrypoint.sh
COPY ./docker/quickstart/test/config.conf /build/config.conf

RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /build
ENTRYPOINT ["entrypoint.sh"]