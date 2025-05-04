FROM ubuntu:noble AS odyssey-build-env

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake \
    postgresql-server-dev-all

FROM odyssey-build-env AS odyssey-build

ARG odyssey_build_type

RUN mkdir build_dir
COPY . /build_dir

WORKDIR /build_dir
RUN make ${odyssey_build_type}

FROM golang:1.23-alpine AS golang-tests-builder

RUN mkdir -p /ody_integration_test
RUN mkdir -p /prep_stmts
RUN mkdir -p /config-validation

COPY ./docker/ody_integration_test /ody_integration_test
COPY ./docker/prep_stmts /prep_stmts
COPY ./docker/config-validation /config-validation

WORKDIR /ody_integration_test
RUN go mod download && cd pkg && CGO_ENABLED=0 go build -o ody_integration_test

WORKDIR /prep_stmts
RUN go mod download && cd pkg && CGO_ENABLED=0 go build -o pstmts-test

WORKDIR /config-validation
RUN go mod download && cd pkg && go build -o config-validation

FROM ubuntu:noble AS dotnet-tests-builder

RUN apt-get update && apt-get install -y --no-install-recommends software-properties-common

RUN add-apt-repository ppa:dotnet/backports

RUN apt-get update && apt-get install -y --no-install-recommends \
    dotnet-sdk-9.0 dotnet-runtime-9.0

COPY ./docker/npgsql_compat /npgsql_compat
RUN cd /npgsql_compat && dotnet build

FROM ubuntu:noble AS postgres-setup

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo \
    postgresql-16 \
    postgresql-common \
    postgresql-server-dev-16

COPY ./docker/bin/setup /usr/bin/setup
COPY ./docker/bin/teardown /usr/bin/teardown
COPY ./docker/bin/ody-stop /usr/bin/ody-stop

RUN /usr/bin/setup
RUN /usr/bin/teardown

FROM postgres-setup AS test-runner

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl software-properties-common
RUN install -m 0755 -d /etc/apt/keyrings
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
RUN chmod a+r /etc/apt/keyrings/docker.asc
RUN echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
    $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
    tee /etc/apt/sources.list.d/docker.list > /dev/null

RUN add-apt-repository ppa:dotnet/backports

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    openssl \
    cmake \
    clang \
    gdb \
    strace \
    ltrace \
    bpftrace \
    ldap-utils \
    python3 \
    postgresql-16 \
    postgresql-common \
    postgresql-server-dev-16 \
    sudo \
    git \
    vim \
    docker-ce docker-ce-cli containerd.io \
    dotnet-sdk-9.0 dotnet-runtime-9.0 \
    openssh-server

# Taken from - https://docs.docker.com/engine/examples/running_ssh_service/#environment-variables

RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN echo 'PermitRootLogin yes' > /etc/ssh/sshd_config.d/10.myconf.conf

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE="in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# 22 for ssh server. 7777 for gdb server.
EXPOSE 22 7777

RUN useradd -ms /bin/bash debugger
RUN echo 'debugger:pwd' | chpasswd

RUN mkdir /tmp/odyssey

COPY ./docker/pg/pg_hba-test.conf /etc/postgresql/16/main/pg_hba.conf

COPY ./docker/odyssey.conf /etc/odyssey/odyssey.conf
COPY ./docker/lagpolling/lag-conf.conf /etc/odyssey/lag-conf.conf
COPY ./docker/prep_stmts/pstmts.conf /etc/odyssey/pstmts.conf
COPY ./docker/config-validation/configs /etc/odyssey/configs
COPY ./docker/reload/config.conf /etc/odyssey/reload-config.conf

COPY ./docker/bin/ody-restart /usr/bin/ody-restart
COPY ./docker/bin/ody-start  /usr/bin/ody-start
COPY ./docker/bin/ody-stop /usr/bin/ody-stop
COPY ./docker/bin/start-pg /usr/bin/start-pg

COPY ./docker/scram /scram
COPY ./docker/hba /hba
COPY ./docker/rule_address /rule_address
COPY ./docker/auth_query /auth_query
COPY ./docker/ldap /ldap
COPY ./docker/lagpolling /lagpolling
COPY ./docker/shell-test /shell-test
COPY ./docker/tsa /tsa
COPY ./docker/group /group
COPY ./docker/xproto /xproto
COPY ./docker/copy /copy
COPY ./docker/gorm /gorm
COPY ./docker/reload /reload
COPY ./docker/tls-compat /tls-compat
COPY ./docker/invalid_log_file /invalid_log_file

COPY ./third_party/machinarium/gdb/machinarium-gdb.py /gdb.py

COPY --from=golang-tests-builder /ody_integration_test/pkg/ody_integration_test /ody_integration_test
COPY --from=golang-tests-builder /prep_stmts/pkg/pstmts-test /pstmts-test
COPY --from=golang-tests-builder /config-validation/pkg/config-validation /config-validation

COPY --from=dotnet-tests-builder /npgsql_compat /npgsql_compat

COPY --from=odyssey-build /build_dir/build/sources/odyssey /usr/bin/odyssey
COPY --from=odyssey-build /build_dir/build/test/odyssey_test /usr/bin/odyssey_test

COPY --from=odyssey-build /build_dir/build/test/machinarium /machinarium

FROM test-runner AS test-entrypoint

COPY ./docker/entrypoint.sh /usr/local/bin/entrypoint.sh

RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

FROM test-runner AS dev-env

ENTRYPOINT ["/usr/sbin/sshd", "-D"]
