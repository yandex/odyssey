FROM fedora:40

RUN dnf install -y cmake gcc make git openssl-devel zlib-devel bison flex libicu-devel readline-devel perl

RUN git clone --depth 1 -b REL_17_STABLE https://github.com/postgres/postgres.git
RUN cd postgres && ./configure --prefix=/usr/lib/postgresql/17 && make -j $(nproc) && make install

ENV PATH="$PATH:/usr/lib/postgresql/17/bin"

COPY . /workdir
WORKDIR /workdir

RUN make build_release
