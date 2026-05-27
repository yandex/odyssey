FROM ubuntu:noble

RUN apt update && apt install -y ca-certificates
RUN sed -i 's|http://archive.ubuntu.com/ubuntu|http://mirror.yandex.ru/ubuntu|g' \
    /etc/apt/sources.list.d/ubuntu.sources && \
    sed -i 's|http://security.ubuntu.com/ubuntu|http://mirror.yandex.ru/ubuntu|g' \
    /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get update -o Acquire::AllowInsecureRepositories=true
RUN apt-get install -y --allow-unauthenticated --no-install-recommends clang-format-18 python3 python-is-python3 git

RUN git clone https://github.com/Sarcasm/run-clang-format.git

COPY docker/format/run-format.sh /run.sh
RUN chmod +x /run.sh

WORKDIR /odyssey

ENTRYPOINT [ "/run.sh" ]
