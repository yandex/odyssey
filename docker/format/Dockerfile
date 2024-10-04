FROM ubuntu:noble

RUN apt-get update
RUN apt-get install -y clang-format-18 python3 python-is-python3 git

RUN git clone https://github.com/Sarcasm/run-clang-format.git

COPY docker/format/run-format.sh /run.sh
RUN chmod +x /run.sh

WORKDIR /odyssey

ENTRYPOINT [ "/run.sh" ]
