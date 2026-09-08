#!/usr/bin/env bash

set -ex

pushd /tests/tls-reload-cert-path/

# Two certificates with distinct CNs, so that the one being served can be
# identified from the wire.
openssl req -x509 -newkey rsa:2048 -keyout alpha.key -out alpha.pem -days 2 -nodes -subj "/CN=odyssey-cert-alpha"
openssl req -x509 -newkey rsa:2048 -keyout bravo.key -out bravo.pem -days 2 -nodes -subj "/CN=odyssey-cert-bravo"

popd

served_cn() {
	echo | openssl s_client -starttls postgres -connect 127.0.0.1:6432 2>/dev/null |
		openssl x509 -noout -subject |
		sed -e 's/.*CN\s*=\s*//'
}

odyssey /tests/tls-reload-cert-path/config.conf
sleep 1

cn=$(served_cn)
if [[ "$cn" != "odyssey-cert-alpha" ]]; then
	echo "expected odyssey-cert-alpha before reload, got '$cn'"
	cat /var/log/odyssey.log
	exit 1
fi

# Point the listener at the other certificate, as a renewal that changes the
# path would.
sed -i 's|alpha\.key|bravo.key|; s|alpha\.pem|bravo.pem|' /tests/tls-reload-cert-path/config.conf

kill -s HUP $(pidof odyssey)
sleep 1

cn=$(served_cn)
if [[ "$cn" != "odyssey-cert-bravo" ]]; then
	echo "expected odyssey-cert-bravo after reload, got '$cn'"
	cat /var/log/odyssey.log
	exit 1
fi

ody-stop
