# Builder for docs
# mount odyssey directory into /odyssey

FROM python:3.12-slim

RUN pip install \
    mkdocs==1.6.1 \
    mkdocs-material==9.6.23 \
    mkdocs-exclude \
    mkdocs-minify-plugin

WORKDIR /odyssey

ENTRYPOINT ["mkdocs", "build", "-s"]
