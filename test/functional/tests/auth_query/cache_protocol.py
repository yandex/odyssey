import select
import socket
import struct
import subprocess
import sys
import time
from contextlib import ExitStack


pg_port, odyssey_port = map(int, sys.argv[1:])


def sql(query):
    return subprocess.check_output(
        ['psql', '-XAt', '-v', 'ON_ERROR_STOP=1', '-h', '127.0.0.1',
         '-p', str(pg_port), '-U', 'postgres', '-d', 'postgres', '-c', query],
        text=True, timeout=5,
    ).strip()


def recv_exact(sock, size):
    result = b''
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise ConnectionError('unexpected EOF')
        result += chunk
    return result


def message(sock):
    header = recv_exact(sock, 5)
    size = struct.unpack('!I', header[1:])[0]
    return header[:1], recv_exact(sock, size - 4)


def prepare(stack, user, database='auth_query_cache_db', peer='127.0.0.1'):
    sock = stack.enter_context(socket.create_connection(
        ('127.0.0.1', odyssey_port), timeout=2, source_address=(peer, 0)))
    params = f'user\0{user}\0database\0{database}\0\0'.encode()
    sock.sendall(struct.pack('!II', len(params) + 8, 196608) + params)
    assert message(sock) == (b'R', struct.pack('!I', 3))
    return sock


def password(sock, value='pw'):
    value = value.encode() + b'\0'
    sock.sendall(b'p' + struct.pack('!I', len(value) + 4) + value)


def expect_failure(sock):
    kind, body = message(sock)
    assert kind == b'E' and b'failed to make auth query' in body, (kind, body)


def check_peers():
    for peer, value, accepted in [
        ('127.0.0.1', 'first', True),
        ('127.0.0.2', 'first', False),
        ('127.0.0.2', 'second', True),
        ('127.0.0.1', 'second', False),
    ]:
        with ExitStack() as stack:
            sock = prepare(stack, 'peer_user', 'auth_query_peer_db', peer)
            password(sock, value)
            kind, body = message(sock)
            if accepted:
                assert (kind, body) == (b'R', struct.pack('!I', 0))
            else:
                assert kind == b'E' and b'password authentication failed' in body, (kind, body)
    print('Cache peer isolation: PASS', flush=True)


def check_overflow(release):
    suffix = 'release' if release else 'timeout'
    prefix = f'cache_busy_{suffix}_'
    user = f'cache_overflow_{suffix}'
    sql(f"INSERT INTO auth_query_cache_test.credentials "
        f"SELECT '{prefix}' || n, 'pw' FROM generate_series(1, 64) n; "
        f"INSERT INTO auth_query_cache_test.credentials VALUES ('{user}', 'pw')")

    with ExitStack() as stack:
        busy = [prepare(stack, f'{prefix}{n}') for n in range(1, 65)]
        overflow = [prepare(stack, user) for _ in range(2)]
        sql('INSERT INTO auth_query_cache_test.gate VALUES (true)')
        try:
            for sock in busy:
                password(sock)
            # Their refreshes remain active after all frontend waits expire.
            for sock in busy:
                expect_failure(sock)
            assert sql("SELECT count(*) FROM auth_query_cache_test.requests "
                       f"WHERE username LIKE '{prefix}%'") == '0'

            started = time.monotonic()
            for sock in overflow:
                password(sock)
            if release:
                assert not select.select(overflow, [], [], 0.1)[0], \
                    'a full cache must wait for an available entry'
                sql('DELETE FROM auth_query_cache_test.gate')
                for sock in overflow:
                    assert message(sock) == (b'R', struct.pack('!I', 0))
                count = sql("SELECT count(*) FROM auth_query_cache_test.requests "
                            f"WHERE username = '{user}'")
                assert count == '1', f'duplicate auth queries: {count}'
            else:
                for sock in overflow:
                    expect_failure(sock)
                elapsed = time.monotonic() - started
                assert elapsed < 1.5, f'cache wait exceeded its budget: {elapsed}'
                assert sql("SELECT count(*) FROM auth_query_cache_test.requests "
                           f"WHERE username = '{user}'") == '0'
        finally:
            sql('DELETE FROM auth_query_cache_test.gate')

        deadline = time.monotonic() + 5
        while sql("SELECT count(*) FROM auth_query_cache_test.requests "
                  f"WHERE username LIKE '{prefix}%'") != '64':
            assert time.monotonic() < deadline, 'refreshes did not finish'
            time.sleep(0.02)
    print(f'Cache overflow {suffix}: PASS', flush=True)


check_peers()
check_overflow(release=True)
check_overflow(release=False)
