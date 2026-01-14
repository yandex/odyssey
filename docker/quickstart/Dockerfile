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

COPY .git ./.git
COPY debian ./debian
COPY Makefile ./
COPY sources ./sources
COPY cmake ./cmake
COPY LICENSE ./
COPY CMakeLists.txt ./

RUN make clean && make build_release

FROM alpine:latest

RUN apk add --no-cache \
    openssl \
    su-exec \
    tini

WORKDIR /app
COPY --from=build /odyssey/build/sources/odyssey .

COPY ./docker/quickstart/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

RUN mkdir /etc/odyssey

RUN addgroup -g 1001 -S odyssey && \
    adduser -S odyssey -u 1001 && \
    chown -R odyssey:odyssey /app && \
    chown odyssey:odyssey /usr/local/bin/entrypoint.sh && \
    chown -R odyssey:odyssey /etc/odyssey

LABEL org.opencontainers.image.source="https://github.com/yandex/odyssey"
LABEL org.opencontainers.image.description="Odyssey Scalable PostgreSQL connection pooler"
LABEL org.opencontainers.image.licenses="BSD-3-Clause"

ENTRYPOINT ["/sbin/tini", "--", "entrypoint.sh"]