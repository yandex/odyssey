FROM golang:1.25.0

RUN apt-get update -y && apt-get install -y postgresql-client postgresql-contrib

RUN mkdir -p /test
WORKDIR /test

COPY ./prometheus/exporter .

COPY ./docker/prom-exporter/entrypoint.sh .
COPY ./docker/prom-exporter/expected.out .

RUN go mod tidy && go build -o exporter

ENTRYPOINT ["/test/entrypoint.sh"]