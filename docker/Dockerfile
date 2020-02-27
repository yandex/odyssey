FROM ubuntu:bionic

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gcc \
    gdb \
    libpam0g-dev \
    libssl-dev \
    postgresql-server-dev-11 \
    valgrind

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
