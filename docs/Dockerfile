FROM python:3.11-slim

RUN pip install mkdocs mkdocs-material

WORKDIR /docs

EXPOSE 80

ENTRYPOINT ["mkdocs", "serve", "-a", "0.0.0.0:80"]
