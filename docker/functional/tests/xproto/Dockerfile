FROM golang:1.24.3

WORKDIR /root
COPY . /root

RUN go mod init prep_stmt_test && \
    go mod tidy

# odyssey can't run TestHintRoutingXproto from spqr proto tests now
ENTRYPOINT go test -skip TestHintRoutingXproto .
