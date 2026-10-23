#!/usr/bin/env python3
"""Protocol regressions against the isolation suite's running Odyssey."""

import ctypes as ct
import ctypes.util
import unittest


pq = ct.CDLL(ct.util.find_library("pq"))


def function(name, result, *args):
    fn = getattr(pq, name)
    fn.restype = result
    fn.argtypes = args
    return fn


pointer = ct.c_void_p
text = ct.c_char_p
integer = ct.c_int
connect = function("PQconnectdb", pointer, text)
finish = function("PQfinish", None, pointer)
status = function("PQstatus", integer, pointer)
error = function("PQerrorMessage", text, pointer)
clear = function("PQclear", None, pointer)
result_status = function("PQresultStatus", integer, pointer)
result_error = function("PQresultErrorMessage", text, pointer)
error_field = function("PQresultErrorField", text, pointer, integer)
ntuples = function("PQntuples", integer, pointer)
nfields = function("PQnfields", integer, pointer)
getvalue = function("PQgetvalue", text, pointer, integer, integer)
exec_query = function("PQexec", pointer, pointer, text)
exec_params = function("PQexecParams", pointer, pointer, text, integer,
                       pointer, pointer, pointer, pointer, integer)
prepare = function("PQprepare", pointer, pointer, text, text, integer, pointer)
describe = function("PQdescribePrepared", pointer, pointer, text)
exec_prepared = function("PQexecPrepared", pointer, pointer, text, integer,
                         pointer, pointer, pointer, integer)
nparams = function("PQnparams", integer, pointer)
paramtype = function("PQparamtype", ct.c_uint, pointer, integer)
fieldtype = function("PQftype", ct.c_uint, pointer, integer)


class ProtocolTests(unittest.TestCase):
    def connection(self, conninfo=b""):
        conn = connect(conninfo)
        self.assertTrue(conn)
        self.addCleanup(finish, conn)
        return conn

    def setUp(self):
        self.conn = self.connection()
        self.assertEqual(status(self.conn), 0, error(self.conn))

    def result(self, result, sqlstate=None):
        self.assertTrue(result)
        try:
            if sqlstate is not None:
                self.assertEqual(error_field(result, ord("C")), sqlstate,
                                 result_error(result))
                return []
            self.assertIn(result_status(result), (1, 2), result_error(result))
            return [[getvalue(result, row, col).decode()
                     for col in range(nfields(result))]
                    for row in range(ntuples(result))]
        finally:
            clear(result)

    def query(self, query, extended=False, sqlstate=None):
        if extended:
            result = exec_params(self.conn, query.encode(), 0,
                                 None, None, None, None, 0)
        else:
            result = exec_query(self.conn, query.encode())
        return self.result(result, sqlstate)

    def assert_tracked(self, definition, expected):
        definitions = [row[1] for row in
                       self.query("SHOW GLOBAL_PREPARED_STATEMENTS")]
        self.assertEqual(definition in definitions, expected, definitions)

    def test_discard_all(self):
        for extended in (False, True):
            with self.subTest(extended=extended):
                definition = "SELECT 1706006"
                self.query("PREPARE discard_me AS " + definition)
                self.assert_tracked(definition, True)

                # Rejection inside a transaction must preserve the SQL maps.
                self.query("BEGIN")
                self.query("DISCARD ALL", extended, sqlstate=b"25001")
                self.query("ROLLBACK")
                self.assert_tracked(definition, True)
                self.assertEqual(self.query("EXECUTE discard_me"), [["1706006"]])

                self.query("DISCARD ALL", extended)
                self.assert_tracked(definition, False)
                self.query("EXECUTE discard_me", sqlstate=b"26000")

    def test_probe_bind_describe_execute(self):
        definition = ("SELECT pg_catalog.pg_isolation_test_session_is_blocked"
                      "($1, '{}')")
        conn = connect(b"")
        self.assertTrue(conn)
        try:
            self.assertEqual(status(conn), 0, error(conn))
            self.result(prepare(conn, b"probe", definition.encode(), 0, None))
            result = describe(conn, b"probe")
            self.assertTrue(result)
            try:
                self.assertEqual(result_status(result), 1, result_error(result))
                self.assertEqual(nparams(result), 1)
                self.assertEqual(paramtype(result, 0), 23)
                self.assertEqual(nfields(result), 1)
                self.assertEqual(fieldtype(result, 0), 16)
            finally:
                clear(result)
            params = (text * 1)(b"0")
            for _ in range(3):
                self.assertEqual(self.result(exec_prepared(
                    conn, b"probe", 1, params, None, None, 0)), [["f"]])
        finally:
            finish(conn)
        # A server round trip lets the disconnected probe client clean up.
        self.query("SELECT pg_sleep(0.1)")
        self.assert_tracked(definition, False)

    def test_rejected_route_keeps_odyssey_alive(self):
        for _ in range(10):
            rejected = self.connection(b"dbname=__odyssey_missing_route__")
            self.assertNotEqual(status(rejected), 0)
            self.assertEqual(self.query("SELECT 1"), [["1"]])


if __name__ == "__main__":
    unittest.main(verbosity=2)
