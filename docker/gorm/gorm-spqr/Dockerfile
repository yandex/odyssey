FROM golang:1.21

WORKDIR /root
COPY . /root

RUN go get ./...

ENTRYPOINT go test github.com/pg-sharding/gorm-spqr/tests
