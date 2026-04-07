FROM golang:1.26.0

WORKDIR /root
COPY . /root

RUN go mod init prep_stmt_test && \
    go mod tidy

# odyssey can't currently run all the SQPR proto tests
ENTRYPOINT SPQR_XPROTO_TEST_PURE_PG_ONLY=yes go test -v .
