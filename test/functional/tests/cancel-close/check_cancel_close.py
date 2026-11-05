#!/usr/bin/env python3
"""Exercise psycopg's new libpq cancellation API over TCP and TLS."""

import asyncio
import unittest

import psycopg


class CancelSafe(unittest.IsolatedAsyncioTestCase):
    @classmethod
    def setUpClass(cls):
        # Falling back to legacy PQcancel would hide reset-on-close errors.
        psycopg.capabilities.has_cancel_safe(check=True)

    async def check_cancel(self, port, sslmode):
        conninfo = (
            f"host=localhost port={port} user=postgres dbname=postgres "
            f"sslmode={sslmode} connect_timeout=5"
        )
        async with (
            await psycopg.AsyncConnection.connect(conninfo, autocommit=True) as conn,
            await psycopg.AsyncConnection.connect(
                conninfo, autocommit=True, prepare_threshold=None
            ) as observer,
        ):
            sql = "SELECT pg_sleep(30) /* cancel-close */"
            query = asyncio.create_task(conn.execute(sql))
            try:
                # Wait for PostgreSQL to start the query before canceling it.
                async with asyncio.timeout(5):
                    while True:
                        cursor = await observer.execute(
                            "SELECT EXISTS (SELECT 1 FROM pg_stat_activity "
                            "WHERE query = %s AND state = 'active')",
                            (sql,),
                        )
                        if (await cursor.fetchone())[0]:
                            break
                        await asyncio.sleep(0.01)

                # A reset or missing TLS close_notify must fail this call.
                await conn.cancel_safe(timeout=5)
                with self.assertRaises(psycopg.errors.QueryCanceled):
                    await asyncio.wait_for(query, timeout=5)

                cursor = await conn.execute("SELECT 42")
                self.assertEqual(await cursor.fetchone(), (42,))
            finally:
                await conn.close()
                await asyncio.gather(query, return_exceptions=True)

    async def test_plain_cancel(self):
        await self.check_cancel(6432, "disable")

    async def test_tls_cancel(self):
        await self.check_cancel(7432, "require")


if __name__ == "__main__":
    unittest.main()
