#!/bin/bash -x

# Tests are based on fact that in case of insufficient privilege
# psql return nothing and all of logging done by odyssey

sleep 1
/usr/bin/odyssey /tests/shell-test/conf.conf

response=$(psql -U rogue -d console -h localhost -p 6432 -c 'show errors;')
if [[ $response != "" ]]; then
  exit 1
fi
response=$(psql -U stat -d console -h localhost -p 6432 -c 'show errors;')
if [[ $response == "" ]]; then
  exit 1
fi
response=$(psql -U stat -d console -h localhost -p 6432 -c 'reload;')
if [[ $response != "" ]]; then
  exit 1
fi
response=$(psql -U admin -d console -h localhost -p 6432 -c 'reload;')
if [[ $response == "" ]]; then
  exit 1
fi

#Change of roles online
mv /tests/shell-test/conf.conf /tests/shell-test/tmp
mv /tests/shell-test/conf2.conf /tests/shell-test/conf.conf
mv /tests/shell-test/tmp /tests/shell-test/conf2.conf

#First call actually reload config, second check what privilege of admin are dropped
response=$(psql -U admin -d console -h localhost -p 6432 -c 'reload;')
response=$(psql -U admin -d console -h localhost -p 6432 -c 'reload;')
if [[ $response != "" ]]; then
  mv /tests/shell-test/conf2.conf /tests/shell-test/tmp
  mv /tests/shell-test/conf.conf /tests/shell-test/conf2.conf
  mv /tests/shell-test/tmp /tests/shell-test/conf.conf
  exit 1
fi
response=$(psql -U admin2 -d console -h localhost -p 6432 -c 'reload;')
if [[ $response == "" ]]; then
  mv /tests/shell-test/conf2.conf /tests/shell-test/tmp
  mv /tests/shell-test/conf.conf /tests/shell-test/conf2.conf
  mv /tests/shell-test/tmp /tests/shell-test/conf.conf
  exit 1
fi

mv /tests/shell-test/conf2.conf /tests/shell-test/tmp
mv /tests/shell-test/conf.conf /tests/shell-test/conf2.conf
mv /tests/shell-test/tmp /tests/shell-test/conf.conf
