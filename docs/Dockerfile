# Builder for docs
# mount odyssey directory into /odyssey

FROM python:3.12-slim

RUN pip install \
    mkdocs \
    mkdocs-material \
    mkdocs-exclude \
    mkdocs-minify-plugin

WORKDIR /odyssey

ENTRYPOINT ["mkdocs", "build", "-s"]
