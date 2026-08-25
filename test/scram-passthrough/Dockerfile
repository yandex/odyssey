FROM odyssey/pg-base AS odyssey-build

ARG build_type=build_dbg
ARG odyssey_cc=gcc

RUN mkdir /build_dir
WORKDIR /build_dir

COPY . .

RUN CC=${odyssey_cc} make clean && make ${build_type}

RUN cp /build_dir/build/sources/odyssey /usr/bin/odyssey
COPY ./sources/machinarium/gdb/machinarium-gdb.py /gdb.py
COPY ./test/functional/bin/ody-stop /usr/bin/ody-stop

FROM odyssey-build AS runner

RUN useradd -ms /bin/bash postgres
RUN mkdir regress
WORKDIR /home/postgres

COPY ./test/scram-passthrough/entrypoint.sh /home/postgres/entrypoint.sh
COPY ./test/scram-passthrough/odyssey.conf /home/postgres/odyssey.conf

RUN chown postgres /home/postgres/entrypoint.sh

USER postgres

ENTRYPOINT ["/home/postgres/entrypoint.sh"]
