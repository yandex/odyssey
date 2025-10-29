FROM ubuntu:latest

RUN apt-get update -o Acquire::AllowInsecureRepositories=true && apt-get install -y --no-install-recommends --allow-unauthenticated \
    python3 \
    postgresql-contrib \
    sudo

COPY ./docker/oom/test/entrytest.sh /usr/local/bin/entrytest.sh
COPY ./docker/functional/bin/ody-stop /usr/local/bin/ody-stop
RUN chmod +x /usr/local/bin/entrytest.sh

ENTRYPOINT ["entrytest.sh"]