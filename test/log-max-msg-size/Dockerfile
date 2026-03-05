FROM alpine:latest

RUN apk add --no-cache postgresql-client bash

RUN mkdir -p /test
WORKDIR /test

COPY ./test/log-max-msg-size/entrypoint.sh .
RUN chmod +x /test/entrypoint.sh

ENTRYPOINT ["/test/entrypoint.sh"]
