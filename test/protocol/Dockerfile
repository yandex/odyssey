FROM golang:1.25.0

RUN apt-get update -y && apt-get install -y postgresql-client

COPY ./test/protocol /protocol-test
WORKDIR /protocol-test

RUN go mod tidy

ENTRYPOINT ["/protocol-test/entrytest.sh"]