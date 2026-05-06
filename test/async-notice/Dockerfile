FROM golang:1.25.0

RUN apt-get update -y && apt-get install -y postgresql-client postgresql-contrib

RUN mkdir -p /test
WORKDIR /test

COPY ./test/async-notice/entrypoint.sh .
COPY ./test/async-notice/go.mod .
COPY ./test/async-notice/main_test.go .

RUN go mod tidy

ENTRYPOINT ["/test/entrypoint.sh"]