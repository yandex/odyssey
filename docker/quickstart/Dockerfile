FROM debian:trixie-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    make \
    git \
    libssl-dev \
    libpam0g-dev \
    libldap-dev \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /odyssey
WORKDIR /odyssey

COPY . ./

RUN make clean && make build_release

FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    libldap-dev \
    libpam0g \
    tini \
    gosu \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /odyssey/build/sources/odyssey .

COPY ./docker/quickstart/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

RUN mkdir /etc/odyssey

RUN groupadd -g 1001 odyssey && \
    useradd -u 1001 -g odyssey -s /sbin/nologin -M odyssey && \
    chown -R odyssey:odyssey /app && \
    chown odyssey:odyssey /usr/local/bin/entrypoint.sh && \
    chown -R odyssey:odyssey /etc/odyssey

LABEL org.opencontainers.image.source="https://github.com/yandex/odyssey"
LABEL org.opencontainers.image.description="Odyssey Scalable PostgreSQL connection pooler"
LABEL org.opencontainers.image.licenses="BSD-3-Clause"

ENTRYPOINT ["/usr/bin/tini", "--", "entrypoint.sh"]
