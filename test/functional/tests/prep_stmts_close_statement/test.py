#!/usr/bin/env python3
"""
Test: Verify Odyssey handles Close message with 'S' option (prepared statement close)

This test ensures that when a client sends a Close message in extended protocol
with type 'S' (prepared statement), Odyssey removes the statement from
client->prep_stmt_ids cache.

Uses raw PostgreSQL protocol via psycopg3's internals.
"""
import sys
import socket
import struct

conninfo_parts = {
    "host": "localhost",
    "port": 6432,
    "user": "postgres",
    "database": "postgres"
}

print("=== Odyssey Close Statement ('S' option) Test ===")
print(f"Connection: {conninfo_parts}")


def send_bytes(sock, data):
    """Send bytes to socket"""
    sock.sendall(data)


def recv_bytes(sock, n):
    """Receive exactly n bytes from socket"""
    data = b''
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise Exception("Connection closed")
        data += chunk
    return data


def recv_message(sock):
    """Receive a single PostgreSQL protocol message"""
    header = recv_bytes(sock, 5)
    msg_type = chr(header[0])
    msg_len = struct.unpack('>I', header[1:5])[0]
    body = recv_bytes(sock, msg_len - 4) if msg_len > 4 else b''
    return msg_type, body


def recv_until_ready(sock):
    """Receive messages until ReadyForQuery"""
    messages = []
    while True:
        msg_type, body = recv_message(sock)
        messages.append((msg_type, body))
        if msg_type == 'Z':  # ReadyForQuery
            break
        if msg_type == 'E':  # ErrorResponse
            # Parse error message
            error_msg = body.decode('utf-8', errors='replace')
            print(f"Error received: {error_msg}")
    return messages


def make_startup_message(user, database):
    """Create startup message"""
    params = f"user\x00{user}\x00database\x00{database}\x00\x00"
    msg = struct.pack('>I', len(params) + 8) + struct.pack('>I', 196608) + params.encode()
    return msg


def make_query_message(query):
    """Create simple query message"""
    query_bytes = query.encode() + b'\x00'
    return b'Q' + struct.pack('>I', len(query_bytes) + 4) + query_bytes


def make_parse_message(stmt_name, query):
    """Create Parse message"""
    stmt_bytes = stmt_name.encode() + b'\x00'
    query_bytes = query.encode() + b'\x00'
    # No parameter types
    body = stmt_bytes + query_bytes + struct.pack('>H', 0)
    return b'P' + struct.pack('>I', len(body) + 4) + body


def make_bind_message(portal_name, stmt_name):
    """Create Bind message"""
    portal_bytes = portal_name.encode() + b'\x00'
    stmt_bytes = stmt_name.encode() + b'\x00'
    # No parameters, no result formats
    body = portal_bytes + stmt_bytes + struct.pack('>H', 0) + struct.pack('>H', 0) + struct.pack('>H', 0)
    return b'B' + struct.pack('>I', len(body) + 4) + body


def make_execute_message(portal_name, max_rows=0):
    """Create Execute message"""
    portal_bytes = portal_name.encode() + b'\x00'
    body = portal_bytes + struct.pack('>I', max_rows)
    return b'E' + struct.pack('>I', len(body) + 4) + body


def make_close_statement_message(stmt_name):
    """Create Close message with type 'S' (prepared statement)"""
    stmt_bytes = stmt_name.encode() + b'\x00'
    body = b'S' + stmt_bytes
    return b'C' + struct.pack('>I', len(body) + 4) + body


def make_sync_message():
    """Create Sync message"""
    return b'S' + struct.pack('>I', 4)


def make_describe_statement_message(stmt_name):
    """Create Describe message for statement"""
    stmt_bytes = stmt_name.encode() + b'\x00'
    body = b'S' + stmt_bytes
    return b'D' + struct.pack('>I', len(body) + 4) + body


try:
    # Connect
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((conninfo_parts["host"], conninfo_parts["port"]))

    # Send startup
    send_bytes(sock, make_startup_message(conninfo_parts["user"], conninfo_parts["database"]))

    # Receive authentication and ReadyForQuery
    messages = recv_until_ready(sock)
    print("Connected to Odyssey")

    # Create a temp table using simple query
    send_bytes(sock, make_query_message("CREATE TEMP TABLE _close_test (id int)"))
    messages = recv_until_ready(sock)
    print("Created temp table")

    send_bytes(sock, make_query_message("INSERT INTO _close_test VALUES (1)"))
    messages = recv_until_ready(sock)
    print("Inserted data")

    # Parse a named prepared statement using extended protocol
    stmt_name = "test_stmt"
    print(f"Parsing prepared statement '{stmt_name}'...")
    send_bytes(sock, make_parse_message(stmt_name, "SELECT * FROM _close_test"))
    send_bytes(sock, make_sync_message())
    messages = recv_until_ready(sock)

    # Check for ParseComplete
    has_parse_complete = any(m[0] == '1' for m in messages)
    if has_parse_complete:
        print("ParseComplete received - statement created")
    else:
        print("ERROR: ParseComplete not received")
        sys.exit(1)

    # Execute the statement to verify it works
    print("Executing prepared statement...")
    send_bytes(sock, make_bind_message("", stmt_name))
    send_bytes(sock, make_execute_message(""))
    send_bytes(sock, make_sync_message())
    messages = recv_until_ready(sock)

    has_data = any(m[0] == 'D' for m in messages)  # DataRow
    if has_data:
        print("Execute OK - got data")

    # Now close the statement using Close message with 'S' type
    print(f"Sending Close message for statement '{stmt_name}' (type 'S')...")
    send_bytes(sock, make_close_statement_message(stmt_name))
    send_bytes(sock, make_sync_message())
    messages = recv_until_ready(sock)

    # Check for CloseComplete
    has_close_complete = any(m[0] == '3' for m in messages)
    if has_close_complete:
        print("CloseComplete received - statement closed")
    else:
        print("ERROR: CloseComplete not received")
        for m in messages:
            print(f"  Message: {m[0]}")
        sys.exit(1)

    # Try to execute the closed statement - should fail
    print("Attempting to bind to closed statement (should fail)...")
    send_bytes(sock, make_bind_message("", stmt_name))
    send_bytes(sock, make_sync_message())
    messages = recv_until_ready(sock)

    # Check for error
    has_error = any(m[0] == 'E' for m in messages)
    if has_error:
        print("Correctly got error - statement was closed")
        print("\n=== TEST PASSED ===")
        sys.exit(0)
    else:
        print("ERROR: No error received, statement should have been closed")
        sys.exit(1)

except Exception as e:
    print(f"ERROR: Unexpected exception: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
finally:
    try:
        sock.close()
    except:
        pass

