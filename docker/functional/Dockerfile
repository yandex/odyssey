FROM ubuntu:noble AS odyssey-build-env

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
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
WORKDIR /build_dir

COPY . .

RUN make ${odyssey_build_type}

FROM golang:1.23-alpine AS golang-tests-builder

RUN mkdir -p /ody_integration_test
RUN mkdir -p /prep_stmts
RUN mkdir -p /config-validation

COPY ./docker/functional/tests/ody_integration_test /ody_integration_test
COPY ./docker/functional/tests/prep_stmts /prep_stmts
COPY ./docker/functional/tests/config-validation /config-validation

WORKDIR /ody_integration_test
RUN go mod download && cd pkg && CGO_ENABLED=0 go build -o ody_integration_test

WORKDIR /prep_stmts
RUN go mod download && cd pkg && CGO_ENABLED=0 go build -o pstmts-test

WORKDIR /config-validation
RUN go mod download && cd pkg && go build -o config-validation

FROM ubuntu:noble AS dotnet-tests-builder

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    software-properties-common

RUN add-apt-repository ppa:dotnet/backports

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    dotnet-sdk-9.0 dotnet-runtime-9.0

COPY ./docker/functional/tests/npgsql_compat /npgsql_compat
RUN cd /npgsql_compat && dotnet build

FROM ubuntu:noble AS postgres-setup

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    sudo \
    postgresql-16 \
    postgresql-common \
    postgresql-server-dev-16

COPY ./docker/functional/bin/setup /usr/bin/setup
COPY ./docker/functional/bin/teardown /usr/bin/teardown

RUN /usr/bin/setup
RUN /usr/bin/teardown

FROM postgres-setup AS test-runner

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated ca-certificates curl software-properties-common
RUN install -m 0755 -d /etc/apt/keyrings
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
RUN chmod a+r /etc/apt/keyrings/docker.asc
RUN echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
    $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
    tee /etc/apt/sources.list.d/docker.list > /dev/null

RUN add-apt-repository ppa:dotnet/backports

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
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
    lsof \
    postgresql-16 \
    postgresql-common \
    python3 python3-pip python3-venv \
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
RUN mkdir /tests

COPY ./docker/functional/tests/pg/pg_hba-test.conf /etc/postgresql/16/main/pg_hba.conf

COPY ./docker/functional/odyssey.conf /etc/odyssey/odyssey.conf

COPY ./docker/functional/bin/ody-restart /usr/bin/ody-restart
COPY ./docker/functional/bin/ody-start  /usr/bin/ody-start
COPY ./docker/functional/bin/ody-stop /usr/bin/ody-stop
COPY ./docker/functional/bin/start-pg /usr/bin/start-pg

COPY ./docker/functional/tests /tests

WORKDIR /tests

COPY ./third_party/machinarium/gdb/machinarium-gdb.py /gdb.py

COPY --from=golang-tests-builder /ody_integration_test/pkg/ody_integration_test ./ody_integration_test/ody_integration_test
COPY --from=golang-tests-builder /prep_stmts/pkg/pstmts-test ./prep_stmts/pstmts-test
COPY --from=golang-tests-builder /config-validation/pkg/config-validation ./config-validation/config-validation

COPY --from=dotnet-tests-builder /npgsql_compat/src/NpgsqlOdysseyScram.Console/bin/Debug/net9.0/* ./npgsql_compat/

COPY --from=odyssey-build /build_dir/build/sources/odyssey /usr/bin/odyssey
COPY --from=odyssey-build /build_dir/build/test/odyssey_test /usr/bin/odyssey_test

COPY --from=odyssey-build /build_dir/build/test/machinarium /machinarium

COPY --from=odyssey-build /build_dir /build_dir

COPY ./docker/functional/requirements.txt .

RUN python3 -m venv /opt/tests/venv
ENV PATH="/opt/tests/venv/bin:$PATH"

RUN pip install -Ur requirements.txt

COPY ./docker/functional/test_functional.py .

FROM test-runner AS functional-entrypoint

RUN mkdir /test-reports

WORKDIR /tests

ENTRYPOINT ["pytest", "-s", "-vv"]

FROM test-runner AS dev-env

ENTRYPOINT ["/usr/sbin/sshd", "-D"]
