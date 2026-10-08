# single-threaded burst: N clients connect at once, run one query, disconnect
# usage: burst.py <require|disable> <count> [port]
# prints: <start_ns> <end_ns> <1 if all ok else 0>
import asyncio
import ssl
import struct
import sys
import time

HOST, PORT = "127.0.0.1", int(sys.argv[3]) if len(sys.argv) > 3 else 16432
MODE, COUNT = sys.argv[1], int(sys.argv[2])

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE


async def read_until_ready(reader):
    while True:
        hdr = await reader.readexactly(5)
        kind, size = hdr[:1], struct.unpack("!i", hdr[1:])[0]
        body = await reader.readexactly(size - 4)
        if kind == b"E":
            raise RuntimeError(body)
        if kind == b"Z":
            return


async def client():
    reader, writer = await asyncio.open_connection(HOST, PORT)
    if MODE == "require":
        writer.write(struct.pack("!ii", 8, 80877103))
        await writer.drain()
        assert await reader.readexactly(1) == b"S"
        await writer.start_tls(ctx)
    params = b"user\0postgres\0database\0postgres\0\0"
    writer.write(struct.pack("!ii", 8 + len(params), 196608) + params)
    await read_until_ready(reader)
    q = b"select 67;\0"
    writer.write(b"Q" + struct.pack("!i", 4 + len(q)) + q)
    await read_until_ready(reader)
    writer.write(b"X" + struct.pack("!i", 4))
    await writer.drain()
    writer.close()
    return True


async def main():
    start = time.time_ns()
    res = await asyncio.gather(*(client() for _ in range(COUNT)),
                               return_exceptions=True)
    end = time.time_ns()
    ok = all(r is True for r in res)
    if not ok:
        print([r for r in res if r is not True][:3], file=sys.stderr)
    print(start, end, int(ok))


asyncio.run(main())
