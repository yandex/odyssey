FROM ubuntu:noble AS odyssey-build-env

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake \
    postgresql-server-dev-16

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

FROM ubuntu:noble AS test-runner

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
    dotnet-sdk-9.0 dotnet-runtime-9.0

RUN mkdir /tmp/odyssey

COPY ./docker/pg/pg_hba-test.conf /etc/postgresql/16/main/pg_hba.conf

COPY ./docker/odyssey.conf /etc/odyssey/odyssey.conf
COPY ./docker/lagpolling/lag-conf.conf /etc/odyssey/lag-conf.conf
COPY ./docker/prep_stmts/pstmts.conf /etc/odyssey/pstmts.conf
COPY ./docker/config-validation/configs /etc/odyssey/configs
COPY ./docker/reload/config.conf /etc/odyssey/reload-config.conf

COPY --from=golang-tests-builder /ody_integration_test/pkg/ody_integration_test /ody_integration_test
COPY --from=golang-tests-builder /prep_stmts/pkg/pstmts-test /pstmts-test
COPY --from=golang-tests-builder /config-validation/pkg/config-validation /config-validation

COPY --from=odyssey-build /build_dir/build/sources/odyssey /usr/bin/odyssey
COPY --from=odyssey-build /build_dir/build/test/odyssey_test /usr/bin/odyssey_test

COPY --from=odyssey-build /build_dir/build/test/machinarium /machinarium

COPY ./docker/bin/ody-restart /usr/bin/ody-restart
COPY ./docker/bin/ody-start  /usr/bin/ody-start
COPY ./docker/bin/ody-stop /usr/bin/ody-stop
COPY ./docker/bin/setup /usr/bin/setup
COPY ./docker/bin/teardown /usr/bin/teardown

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

COPY ./third_party/machinarium/gdb/machinarium-gdb.py /gdb.py

COPY ./docker/npgsql_compat /npgsql_compat
RUN cd /npgsql_compat && dotnet build

FROM test-runner AS entrypoint

COPY ./docker/entrypoint.sh /usr/local/bin/entrypoint.sh

RUN chmod a+x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
