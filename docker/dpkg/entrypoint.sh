#!/bin/bash

set -ex

export DEBIAN_FRONTEND=noninteractive
export TZ=Europe/Moskow
sudo bash -c "echo $TZ > /etc/timezone"

/root/odys/dpkg/tzdata.sh

#compile pg
#======================================================================================================================================
cd /pdbuild/tmp

mk-build-deps  --build-dep --install --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' debian/control
./configure --prefix=/pgbin && make -j12 && make install -j12

#======================================================================================================================================


#compile odyssey (Scalable postgresql connection pooler)

cd /root/odys

sed -i 's/postgresql-server-dev-13,//g' scripts/debian/control
sed -i 's/libpq-dev (>= 13~~),//g' scripts/debian/control

VERSION=$VERSION BUILD_NUMBER=$BUILD_NUMBER timeout 300 cmake -S -DBUILD_DEBIAN=1 -DCMAKE_BUILD_TYPE=Release -DPOSTGRESQL_LIBRARY=/pgbin/lib/libpgcommon.a -DPOSTGRESQL_LIBPGPORT=/pgbin/lib/libpgport.a -DPOSTGRESQL_INCLUDE_DIR=/pgbin/include/postgresql/server -DPQ_LIBRARY=/pgbin/lib/libpq.a .

#======================================================================================================================================

mk-build-deps  --build-dep --install --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' debian/control

dpkg-buildpackage -us -uc

#======================================================================================================================================
# here we should get some files at /root
