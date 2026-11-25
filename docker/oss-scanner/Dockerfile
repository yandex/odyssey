# OSS Scanner (red.anthropic.com/oss-scanner) build of Odyssey.  This
# should remain aligned with CI, building Odyssey with as many optional
# features as practical.
#
# github.com/anthropics/oss-scanner/blob/main/projects/odyssey/project.yaml
# directs the scanner to use this file.  Authority on writing this file:
# github.com/anthropics/oss-scanner/blob/main/templates/Dockerfile
#
# Test changes to this file as follows, editing the MY_* vars.
# oss-scanner/tools/check insists on an https:// URL, hence the use of the git
# insteadOf mechanism to trick it into testing a local repo.
#
# MY_REPO=$HOME/src/odyssey
# MY_BRANCH=dockerfile-oss-scanner
# git clone https://github.com/anthropics/oss-scanner/
# sed -i "s|^repo:.*|repo: https://local.invalid/odyssey#$MY_BRANCH|" \
#   oss-scanner/projects/odyssey/project.yaml
# GIT_CONFIG_COUNT=2 \
#   GIT_CONFIG_KEY_0="url.file://$MY_REPO.insteadOf" \
#   GIT_CONFIG_VALUE_0="https://local.invalid/odyssey" \
#   GIT_CONFIG_KEY_1="protocol.file.allow" \
#   GIT_CONFIG_VALUE_1="always" \
#   oss-scanner/tools/check odyssey

# Mirror CI (test/build-test/Dockerfile.debian, "Debian build / trixie" job)
FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        git \
        sudo \
        cmake \
        bison \
        flex \
        build-essential \
        libssl-dev \
        openssl \
        libpam0g-dev \
        libldap-dev \
        libldap-common \
        libsystemd-dev

# Odyssey drops privileges itself, so make an unprivileged user for build and
# smoke test.  The scan runs git as root, so configure it to accept the
# non-root-owned /src as safe.
RUN useradd --create-home --shell /bin/bash odyssey && \
    git config --system --add safe.directory /src

# scanner contract: the checkout lives inside the image, at /src
COPY --chown=odyssey:odyssey . /src
WORKDIR /src
USER odyssey

# Per scanner contract, use -g -fno-omit-frame-pointer
ENV CFLAGS='-O2 -g -fno-omit-frame-pointer'
ENV CXXFLAGS=$CFLAGS

# Build like CI jobs "make build_relwithdbginfo", adding optional
# systemd/PAM/LDAP support the way CI build images do.
RUN mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo && \
    make -j"$(nproc)"

# Smoke test like CI "console --test": validate startup configuration rules
# against both a console-only database and a regular transaction-pooled
# forwarding database.  --test only parses and validates the config, so no
# PostgreSQL backend is needed inside the image.
RUN printf '%s\n' \
        'log_format "%p %t %l (%c) %m\n"' \
        'log_to_stdout yes' \
        'unix_socket_dir "/tmp"' \
        'unix_socket_mode "0644"' \
        'locks_dir "/tmp"' \
        'daemonize no' \
        'listen {' \
        '	host "127.0.0.1"' \
        '	port 16432' \
        '}' \
        'storage "local" {' \
        '	type "local"' \
        '}' \
        'storage "postgres_server" {' \
        '	type "remote"' \
        '	host "127.0.0.1"' \
        '	port 5432' \
        '}' \
        'database "console" {' \
        '	user "console" {' \
        '		authentication "none"' \
        '		storage "local"' \
        '		pool "session"' \
        '		pool_size 1' \
        '	}' \
        '}' \
        'database "postgres" {' \
        '	user "postgres" {' \
        '		authentication "none"' \
        '		storage "postgres_server"' \
        '		storage_user "postgres"' \
        '		storage_password ""' \
        '		pool "transaction"' \
        '		pool_size 10' \
        '	}' \
        '}' > /tmp/odyssey-scanner.conf && \
    ./build/sources/odyssey --version && \
    ./build/sources/odyssey --test /tmp/odyssey-scanner.conf

USER root
