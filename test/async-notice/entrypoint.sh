#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 0.1
done

until pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
  echo "Wait for odyssey..."
  sleep 0.1
done

DATABASE_URL='postgres://postgres@odyssey:6432/postgres?sslmode=disable' go test -v .
