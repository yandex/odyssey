#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 0.1
done

/usr/bin/odyssey /etc/odyssey.conf &
ODY_PID=$!

until pg_isready -h 127.0.0.1 -p 6432 -U postgres -d postgres; do
  echo "Wait for odyssey..."
  sleep 0.1
done

do_test() {
  local user="$1"

  SPQR_XPROTO_TEST_PURE_PG_ONLY=yes \
  POSTGRES_HOST=127.0.0.1 \
  POSTGRES_PORT=6432 \
  POSTGRES_DB=postgres \
  POSTGRES_USER="$user" \
  /usr/bin/proto.test \
      -test.v \
      -test.run 'Test[^(DeallocatePrepareRemovesPstmtsByXproto|DeallocateRemovesPstmtsByXproto)]'
}

do_test suser

do_test tuser

ody-stop