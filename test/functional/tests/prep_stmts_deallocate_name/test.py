#!/usr/bin/env python3
"""
Test: Verify Odyssey handles DEALLOCATE name (specific statement)

This test ensures that when a client sends DEALLOCATE stmt_name,
Odyssey removes only that specific statement from client->prep_stmt_ids cache,
while keeping other prepared statements intact.

Uses simple query protocol for PREPARE/EXECUTE/DEALLOCATE.
"""
import sys
import psycopg

conninfo = "host=localhost port=6432 user=postgres dbname=postgres"

print("=== Odyssey DEALLOCATE name Test ===")
print(f"Connection: {conninfo}")

try:
    with psycopg.connect(conninfo, autocommit=True) as conn:
        # Create temp table
        conn.execute("CREATE TEMP TABLE _deallocate_test (id int PRIMARY KEY, val text)")
        conn.execute("INSERT INTO _deallocate_test VALUES (1, 'test1'), (2, 'test2')")

        # Create two named prepared statements using simple query protocol
        print("Creating prepared statements stmt1 and stmt2...")
        conn.execute("PREPARE stmt1 AS SELECT * FROM _deallocate_test WHERE id = $1")
        conn.execute("PREPARE stmt2 AS SELECT * FROM _deallocate_test WHERE id = $1")

        # Execute both to verify they work
        cur = conn.execute("EXECUTE stmt1(1)")
        row = cur.fetchone()
        print(f"Execute stmt1 OK: {row}")

        cur = conn.execute("EXECUTE stmt2(2)")
        row = cur.fetchone()
        print(f"Execute stmt2 OK: {row}")

        # DEALLOCATE only stmt1
        print("Sending DEALLOCATE stmt1...")
        conn.execute("DEALLOCATE stmt1")

        # stmt1 should fail now
        try:
            conn.execute("EXECUTE stmt1(1)")
            print("ERROR: stmt1 should have been deallocated")
            sys.exit(1)
        except psycopg.errors.InvalidSqlStatementName:
            print("Correctly got error for stmt1: statement was deallocated")

        # stmt2 should still work
        cur = conn.execute("EXECUTE stmt2(2)")
        row = cur.fetchone()
        print(f"Execute stmt2 still OK: {row}")

        print("\n=== TEST PASSED ===")
        sys.exit(0)

except Exception as e:
    print(f"ERROR: Unexpected exception: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

