#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 0.1
done

trap 'ody-stop || exit 1' EXIT
ody-start /etc/odyssey.conf console console

do_test() {
  local user="$1"

  SPQR_XPROTO_TEST_PURE_PG_ONLY=yes \
  POSTGRES_HOST=127.0.0.1 \
  POSTGRES_PORT=6432 \
  POSTGRES_DB=postgres \
  POSTGRES_USER="$user" \
  /usr/bin/proto.test \
      -test.v
}

do_test suser

do_test tuser

ody-stop