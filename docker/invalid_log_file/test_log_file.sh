#!/bin/bash -x

set -ex
if ! /usr/bin/odyssey /invalid_log_file/config.conf | grep -q "failed to open log file "; then 
	echo "ERROR: can't find log open error in stdout"
    exit 1
fi

ody-stop