"""Integration checks for the PgBouncer configuration converter."""

import os
import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
ODYSSEY = pathlib.Path(os.environ.get("ODYSSEY_BIN", ROOT / "build/sources/odyssey"))


class ConverterTest(unittest.TestCase):
    def convert(self, ini, users=None):
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            source = directory / "pgbouncer.ini"
            source.write_text(ini)
            if users is not None:
                (directory / "userlist.txt").write_text(users)
            result = subprocess.run(
                [str(ODYSSEY), "--convert-pgbouncer", str(source)],
                capture_output=True,
                text=True,
                cwd=directory,
            )
            if result.returncode == 0:
                generated = directory / "odyssey.conf"
                generated.write_text(result.stdout)
                checked = subprocess.run(
                    [str(ODYSSEY), "--test", str(generated)],
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
            return result

    def test_md5_database_and_user_mapping(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
listen_port = 6433
auth_type = md5
auth_file = userlist.txt
pool_mode = transaction
default_pool_size = 23
max_client_conn = 500

[databases]
app = host=localhost port=5433 dbname=realapp pool_size=7
""",
            '"alice" "md500000000000000000000000000000000"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        for fragment in (
            'host "127.0.0.1"',
            "port 6433",
            "client_max 500",
            'storage "pgb_1"',
            'host "localhost"',
            "port 5433",
            'database "app"',
            'user "alice"',
            'authentication "md5"',
            'password "md500000000000000000000000000000000"',
            'storage_db "realapp"',
            'pool "transaction"',
            "pool_size 7",
        ):
            self.assertIn(fragment, result.stdout)

    def test_trust_only_allows_listed_users(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = trust
auth_file = userlist.txt

[databases]
app = host=localhost
""",
            '"alice" "unused"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('user "alice"', result.stdout)
        self.assertIn('authentication "none"', result.stdout)
        self.assertIn('password "unused"', result.stdout)
        self.assertIn("client_max 100", result.stdout)
        self.assertIn('unix_socket_dir "/tmp"', result.stdout)
        self.assertIn('unix_socket_mode "0777"', result.stdout)
        self.assertIn('tls "prefer"', result.stdout)
        self.assertEqual(result.stdout.count("listen {"), 2)
        self.assertNotIn("user default", result.stdout)

    def test_quoted_database_name_is_unquoted(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = trust
auth_file = userlist.txt
[databases]
"foo-bar" = host=localhost
""",
            '"alice" "unused"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('database "foo-bar"', result.stdout)
        self.assertNotIn('database "\\"foo-bar\\""', result.stdout)

    def test_unsupported_option_fails_without_partial_config(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = trust
auth_file = userlist.txt
auth_query = select secret
[databases]
app = host=localhost
""",
            '"alice" "unused"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("auth_query", result.stderr)

    def test_statement_pooling_is_rejected(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = trust
auth_file = userlist.txt
pool_mode = statement
[databases]
app = host=localhost
""",
            '"alice" "unused"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("statement", result.stderr)

    def test_missing_auth_file_is_rejected(self):
        result = self.convert("[pgbouncer]\nlisten_addr = 127.0.0.1\nauth_type = md5\n[databases]\napp = host=localhost\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("auth_file", result.stderr)

    def test_relative_auth_file_uses_process_working_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            source_dir = directory / "source"
            source_dir.mkdir()
            source = source_dir / "pgbouncer.ini"
            source.write_text(
                "[pgbouncer]\nlisten_addr=127.0.0.1\nauth_type=trust\n"
                "auth_file=userlist.txt\n[databases]\napp=host=localhost\n"
            )
            (source_dir / "userlist.txt").write_text('"wrong" "secret"\n')
            (directory / "userlist.txt").write_text('"right" "secret"\n')
            result = subprocess.run(
                [str(ODYSSEY), "--convert-pgbouncer", str(source)],
                capture_output=True,
                text=True,
                cwd=directory,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('user "right"', result.stdout)
            self.assertNotIn('user "wrong"', result.stdout)

    def test_quoted_connection_password_and_forced_user(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host = localhost dbname='app data' user=service password='secret with spaces'
""",
            '"alice" "client secret"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('storage_db "app data"', result.stdout)
        self.assertIn('storage_user "service"', result.stdout)
        self.assertIn('storage_password "secret with spaces"', result.stdout)

    def test_connection_string_backslash_is_literal(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost user=service password='pa\\ss'
""",
            '"alice" "client secret"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('storage_password "pa\\ss"', result.stdout)

    def test_connection_string_doubled_apostrophe(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost user=service password='pa''ss'
""",
            '"alice" "client secret"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('storage_password "pa\'ss"', result.stdout)

    def test_scram_secret_is_rejected_without_partial_output(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost
""",
            '"alice" "SCRAM-SHA-256$4096:salt$key:key"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("SCRAM", result.stderr)

    def test_invalid_md5_secret_is_rejected(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost
""",
            '"alice" "md5' + 'z' * 32 + '"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("MD5", result.stderr)

    def test_doubled_quotes_and_backslash_in_auth_file(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost
""",
            '"alice" "a""b\\c"\n',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('password "a\\"b\\c"', result.stdout)

    def test_forced_user_with_multiple_clients_is_rejected(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = md5
auth_file = userlist.txt
[databases]
app = host=localhost user=service password=secret
""",
            '"alice" "first"\n"bob" "second"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("forced user", result.stderr)

    def test_abstract_unix_backend_is_rejected(self):
        result = self.convert(
            """[pgbouncer]
listen_addr = 127.0.0.1
auth_type = trust
auth_file = userlist.txt
[databases]
app = host=@postgres
""",
            '"alice" "unused"\n',
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("TCP host", result.stderr)


if __name__ == "__main__":
    unittest.main()
