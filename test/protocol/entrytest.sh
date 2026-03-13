#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

until pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
  echo "Wait for Odyssey..."
  sleep 1
done

ls
go test -v
