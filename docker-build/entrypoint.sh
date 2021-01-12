#/bin/bash

export VERSION=$(git rev-list HEAD --count)-yandex200
export BUILD_NUMBER=$(git rev-parse --short HEAD)

sed -i 's/postgresql-server-dev-13,//g' scripts/debian/control
sed -i 's/libpq-dev (>= 13~~),//g' scripts/debian/control

timeout 300 cmake -S -Bbuild -DBUILD_DEBIAN=1 -DCMAKE_BUILD_TYPE=Release -DPOSTGRESQL_LIBRARY=/pgbin/lib/libpgcommon.a -DPOSTGRESQL_LIBPGPORT=/pgbin/lib/libpgport.a -DPOSTGRESQL_INCLUDE_DIR=/pgbin/include/server -DPQ_LIBRARY=/pgbin/libpq.a

dpkg-buildpackage -us -uc

