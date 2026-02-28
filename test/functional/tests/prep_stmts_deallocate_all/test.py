#!/usr/bin/env python3
"""
Test: Verify Odyssey clears prepared statement cache on DEALLOCATE ALL

This test ensures that when a client sends DEALLOCATE ALL,
Odyssey invalidates both server->prep_stmts and client->prep_stmt_ids caches.

Uses psycopg3 which uses extended query protocol (Parse/Bind/Execute).
"""
import sys
import psycopg

conninfo = "host=localhost port=6432 user=postgres dbname=postgres"

print("=== Odyssey DEALLOCATE ALL Test ===")
print(f"Connection: {conninfo}")

try:
    with psycopg.connect(conninfo, prepare_threshold=0) as conn:
        # Create temp table
        conn.execute("CREATE TEMP TABLE _deallocate_test (id int PRIMARY KEY, val text)")
        conn.execute("INSERT INTO _deallocate_test VALUES (1, 'test')")
        conn.commit()

        with conn.cursor() as cur:
            # First query - creates prepared statement via extended protocol
            cur.execute("SELECT * FROM _deallocate_test WHERE id = %s", (1,))
            row = cur.fetchone()
            print(f"First query OK: {row}")

            # Send DEALLOCATE ALL
            print("Sending DEALLOCATE ALL...")
            conn.execute("DEALLOCATE ALL")

            # Clear psycopg's internal cache so it sends Parse again
            conn._prepared.clear()

            # Second query - should work if Odyssey cleared its cache
            cur.execute("SELECT * FROM _deallocate_test WHERE id = %s", (1,))
            row = cur.fetchone()
            print(f"Second query OK: {row}")
            print("\n=== TEST PASSED ===")
            sys.exit(0)

except psycopg.errors.InvalidSqlStatementName as e:
    print(f"ERROR: {e}")
    print("\n=== TEST FAILED: Odyssey did not clear prep_stmts on DEALLOCATE ALL ===")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: Unexpected exception: {e}")
    sys.exit(1)

