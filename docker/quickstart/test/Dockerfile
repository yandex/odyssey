FROM alpine:latest

RUN apk add --no-cache \
        postgresql-client \
        python3

COPY ./docker/quickstart/test/entrytest.sh /usr/local/bin/entrytest.sh
COPY ./docker/functional/bin/ody-stop /usr/local/bin/ody-stop
RUN chmod +x /usr/local/bin/entrytest.sh

ENTRYPOINT ["entrytest.sh"]