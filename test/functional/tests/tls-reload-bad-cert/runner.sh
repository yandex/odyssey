#!/usr/bin/env bash

set -ex

pushd /tests/tls-reload-bad-cert/

openssl req -x509 -newkey rsa:2048 -keyout good.key -out good.pem -days 2 -nodes -subj "/CN=odyssey-cert-good"
# A second keypair, used to point the config at a key that does not match the
# certificate.
openssl req -x509 -newkey rsa:2048 -keyout other.key -out other.pem -days 2 -nodes -subj "/CN=odyssey-cert-other"

popd

served_cn() {
	echo | openssl s_client -starttls postgres -connect 127.0.0.1:6432 2>/dev/null |
		openssl x509 -noout -subject |
		sed -e 's/.*CN\s*=\s*//'
}

assert_serving_good_cert() {
	cn=$(served_cn)
	if [[ "$cn" != "odyssey-cert-good" ]]; then
		echo "$1: expected odyssey-cert-good, got '$cn'"
		cat /var/log/odyssey.log
		exit 1
	fi
}

odyssey /tests/tls-reload-bad-cert/config.conf
sleep 1

assert_serving_good_cert "before reload"

# A certificate path that does not exist must not replace the working one.
sed -i 's|good\.pem|missing.pem|; s|good\.key|missing.key|' /tests/tls-reload-bad-cert/config.conf
kill -s HUP $(pidof odyssey)
sleep 1

assert_serving_good_cert "after reload with a missing certificate"

# Neither may a certificate that does not match its key.
sed -i 's|missing\.pem|good.pem|; s|missing\.key|other.key|' /tests/tls-reload-bad-cert/config.conf
kill -s HUP $(pidof odyssey)
sleep 1

assert_serving_good_cert "after reload with a mismatched key"

ody-stop
