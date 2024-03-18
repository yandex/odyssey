FROM golang:1.21

WORKDIR /root
COPY . /root

RUN go mod init prep_stmt_test && \
    go mod tidy

ENTRYPOINT go test .
