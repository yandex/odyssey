FROM ubuntu:resolute

RUN apt-get update -y && apt-get install -y postgresql-client postgresql-client-common postgresql-contrib

COPY ./test/replication/entrytest.sh /usr/local/bin/entrytest.sh
COPY ./test/replication/logical_expected.out /logical_expected.out
RUN chmod +x /usr/local/bin/entrytest.sh

ENTRYPOINT ["entrytest.sh"]