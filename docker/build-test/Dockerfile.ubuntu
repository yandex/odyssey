ARG codename=focal
FROM ubuntu:${codename}

ARG postgres_version=13

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y ca-certificates
RUN sed -i 's/archive.ubuntu.com/mirror.yandex.ru/g' /etc/apt/sources.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
	curl lsb-release ca-certificates gnupg

RUN install -d /usr/share/postgresql-common/pgdg && \
    curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail https://www.postgresql.org/media/keys/ACCC4CF8.asc && \
    echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    sudo \
    postgresql-${postgres_version} \
    postgresql-common \
    postgresql-server-dev-${postgres_version} \
    git \
    libssl-dev \
    libldap-common \
    openssl \
    libpam0g-dev \
    libldap-dev \
    build-essential \
    cmake

COPY . /build_dir
WORKDIR /build_dir

ENTRYPOINT [ "bash", "-c", "make $ODYSSEY_BUILD_TYPE" ]
