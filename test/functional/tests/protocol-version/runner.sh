#!/bin/bash

set -euo pipefail

/usr/bin/odyssey /tests/protocol-version/protocol.conf
trap ody-stop EXIT
sleep 1

write_uint32() {
	printf '%b' "$(printf '\\0%03o\\0%03o\\0%03o\\0%03o' \
		$((($1 >> 24) & 255)) $((($1 >> 16) & 255)) \
		$((($1 >> 8) & 255)) $(($1 & 255)))"
}

check_negotiation() {
	local requested=$1 expected=$2 with_option=$3
	local params='user\0postgres\0database\0postgres\0'
	local option=''
	if [ "$with_option" = 1 ]; then
		option='_pq_.test_protocol_negotiation'
		params+="${option}\\0true\\0"
	fi
	params+='\0'
	local params_size
	params_size=$(printf '%b' "$params" | wc -c)
	local response="/tests/protocol-version/response-${requested}-${with_option}"
	local expected_file="${response}.expected"

	# Check the entire NegotiateProtocolVersion message, including major bits
	# and the exact list of unsupported startup options.
	{
		printf 'v'
		local option_size=0
		if [ "$with_option" = 1 ]; then
			option_size=$((${#option} + 1))
		fi
		write_uint32 $((12 + option_size))
		write_uint32 $(((3 << 16) | expected))
		write_uint32 "$with_option"
		if [ "$with_option" = 1 ]; then
			printf '%s\0' "$option"
		fi
	} >"$expected_file"

	exec 3<>/dev/tcp/127.0.0.1/6432
	{
		write_uint32 $((8 + params_size))
		write_uint32 $(((3 << 16) | requested))
		printf '%b' "$params"
		# Terminate after startup so the test leaves no idle client behind.
		printf 'X\0\0\0\4'
	} >&3
	timeout 5 dd bs=1 count="$(wc -c <"$expected_file")" status=none <&3 >"$response"
	exec 3>&-
	cmp "$expected_file" "$response"
}

# A future minor version must negotiate down to 3.2 on the server.
check_negotiation 65535 2 0

# Unsupported startup options must not upgrade a 3.0 client or downgrade 3.2.
check_negotiation 0 0 1
check_negotiation 2 2 1
check_negotiation 65535 2 1

for version in 3.0 3.2; do
	result=$(psql "host=localhost port=6432 user=postgres dbname=postgres min_protocol_version=$version max_protocol_version=$version" \
		-XAt -v ON_ERROR_STOP=1 -c 'SELECT 42')
	test "$result" = 42
done
