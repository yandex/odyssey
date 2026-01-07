FROM ubuntu:latest

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    postgresql-contrib \
    sudo

COPY ./test/oom/entrytest.sh /usr/local/bin/entrytest.sh
RUN chmod +x /usr/local/bin/entrytest.sh

ENTRYPOINT ["entrytest.sh"]