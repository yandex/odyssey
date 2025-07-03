FROM ubuntu:noble AS odyssey-build-env

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    git \
    libssl-dev \
    openssl \
    libpam0g-dev \
    build-essential \
    cmake \
    postgresql-server-dev-all

FROM odyssey-build-env AS odyssey-build

ARG odyssey_build_type

RUN mkdir /build_dir
WORKDIR /build_dir

COPY . .

RUN make clean && make ${odyssey_build_type}

RUN cp /build_dir/build/sources/odyssey /odyssey

COPY ./docker/stress/odyssey.conf /odyssey.conf

FROM ubuntu:noble AS test-runner-env

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    postgresql-client postgresql-client-common postgresql-contrib \
    openssh-server \
    vim \
    gdb \
    build-essential \
    strace ltrace \
    lsof

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

WORKDIR /stress-test

COPY ./third_party/machinarium/gdb/machinarium-gdb.py /gdb.py

COPY --from=odyssey-build /odyssey /odyssey
COPY --from=odyssey-build /odyssey.conf /odyssey.conf

COPY --from=odyssey-build /build_dir /build_dir

COPY ./docker/stress/entrypoint.sh ./
COPY ./docker/stress/stress.sh ./

FROM test-runner-env AS stress-entrypoint

WORKDIR /stress-test

ENTRYPOINT ["/stress-test/entrypoint.sh"]

FROM test-runner-env AS dev-env

WORKDIR /stress-test

ENTRYPOINT ["/usr/sbin/sshd", "-D"]