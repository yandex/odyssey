FROM ubuntu:noble

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /perf
COPY bench_data.h bench_cache.c run_perf.sh ./
RUN chmod +x run_perf.sh

RUN gcc -O2 -o bench_sqli_guard bench_cache.c

ENTRYPOINT ["./run_perf.sh"]
