FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moskow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

ENV PGSRC=/home/postgres/postgres
ENV PGINSTALL=/home/postgres/pgdir
ENV PGDATA=/home/postgres/data

RUN apt-get update -y && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    pkg-config \
    libreadline-dev \
    libicu-dev \
    flex bison \
    zlib1g-dev \
    git

RUN useradd -ms /bin/bash postgres

USER postgres

RUN git clone --depth=5 --single-branch --branch=REL_18_STABLE https://github.com/postgres/postgres $PGSRC

RUN cd $PGSRC && ./configure --prefix=${PGINSTALL} --enable-depend && make -j$(nproc) install

WORKDIR /home/postgres

COPY ./test/pg_installcheck/entrypoint.sh .

EXPOSE 5432

ENTRYPOINT ["/home/postgres/entrypoint.sh"]
