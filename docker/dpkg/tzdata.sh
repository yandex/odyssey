#!/bin/bash

export DEBIAN_FRONTEND=noninteractive
#install tzdata package
# set your timezone
ln -fs /usr/share/zoneinfo/$TZ /etc/localtime
dpkg-reconfigure --frontend noninteractive tzdata

