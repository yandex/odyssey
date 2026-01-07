#!/bin/bash

set -e

/usr/bin/odyssey /tests/show-unix-socket-ports/conf.conf
sleep 1

# Warm up: create some traffic so that servers/clients appear
psql 'host=localhost port=6432 user=postgres dbname=postgres' \
  --quiet --no-align --tuples-only -c 'SELECT 1' > /dev/null 2>&1

# show servers: columns are type,user,database,state,addr,port,local_addr,local_port,...
psql 'host=localhost port=6432 user=postgres dbname=console' \
  --quiet --no-align --tuples-only -F '|' -c 'show servers' | \
  awk -F '|' 'NF>0 {
    port=$6; lport=$8;
    if (port !~ /^-?[0-9]+$/)  { print "non-numeric servers.port: " port  > "/dev/stderr";  exit 1 }
    if (lport !~ /^-?[0-9]+$/) { print "non-numeric servers.local_port: " lport > "/dev/stderr"; exit 1 }
  }'

# show clients: columns are type,user,database,state,storage_user,addr,port,local_addr,local_port,...
psql 'host=localhost port=6432 user=postgres dbname=console' \
  --quiet --no-align --tuples-only -F '|' -c 'show clients' | \
  awk -F '|' 'NF>0 {
    port=$7; lport=$9;
    if (port !~ /^-?[0-9]+$/)  { print "non-numeric clients.port: " port  > "/dev/stderr";  exit 1 }
    if (lport !~ /^-?[0-9]+$/) { print "non-numeric clients.local_port: " lport > "/dev/stderr"; exit 1 }
  }'

ody-stop