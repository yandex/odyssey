FROM alpine:latest

RUN apk add --no-cache postgresql-client

COPY ./entrytest.sh /usr/local/bin/entrytest.sh
RUN chmod +x /usr/local/bin/entrytest.sh

ENTRYPOINT ["entrytest.sh"]