# Copyright 2024 x0tta6bl4 contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Тесты для Odyssey PQC TLS.
"""

import hashlib
import secrets
import socket
import struct
import time
from unittest import TestCase, mock

from src.integration.yandex.odyssey_pqc_tls import (
    ConnectionState,
    OdysseyConnection,
    OdysseyPQCConfig,
    OdysseyPQCMode,
    OdysseyPQCTLS,
    OdysseyStats,
    PQCSession,
    PQCSessionManager,
)


class TestOdysseyPQCConfig(TestCase):
    """Тесты конфигурации Odyssey PQC."""

    def test_default_config(self) -> None:
        """Проверка конфигурации по умолчанию."""
        with mock.patch(
            "src.integration.yandex.odyssey_pqc_tls.HAS_LIBOQS", True
        ):
            config = OdysseyPQCConfig()
            self.assertEqual(
                config.mode, OdysseyPQCMode.HYBRID_X25519_ML_KEM
            )
            self.assertEqual(config.kem_algorithm, "ML-KEM-768")
            self.assertEqual(config.signature_algorithm, "ML-DSA-65")
            self.assertEqual(config.max_connections, 100)
            self.assertEqual(config.connection_timeout, 30.0)
            self.assertEqual(config.session_ttl, 3600)
            self.assertTrue(config.enable_session_resumption)
            self.assertTrue(config.enable_multiplexing)
            self.assertFalse(config.require_client_auth)

    def test_custom_config(self) -> None:
        """Проверка пользовательской конфигурации."""
        with mock.patch(
            "src.integration.yandex.odyssey_pqc_tls.HAS_LIBOQS", True
        ):
            config = OdysseyPQCConfig(
                mode=OdysseyPQCMode.PURE_ML_KEM,
                max_connections=50,
                connection_timeout=10.0,
                session_ttl=1800,
                enable_session_resumption=False,
            )
            self.assertEqual(config.mode, OdysseyPQCMode.PURE_ML_KEM)
            self.assertEqual(config.max_connections, 50)
            self.assertEqual(config.connection_timeout, 10.0)
            self.assertEqual(config.session_ttl, 1800)
            self.assertFalse(config.enable_session_resumption)


class TestPQCSessionManager(TestCase):
    """Тесты менеджера PQC сессий."""

    def setUp(self) -> None:
        """Настройка тестов."""
        self.manager = PQCSessionManager(max_sessions=5, ttl=60)

    def test_create_session(self) -> None:
        """Тест создания сессии."""
        shared_secret = secrets.token_bytes(32)
        session = self.manager.create_session(shared_secret)

        self.assertIsInstance(session, PQCSession)
        self.assertEqual(session.shared_secret, shared_secret)
        self.assertIsNotNone(session.session_id)
        self.assertGreater(session.expires_at, session.created_at)

    def test_get_session(self) -> None:
        """Тест получения сессии."""
        shared_secret = secrets.token_bytes(32)
        session = self.manager.create_session(shared_secret)

        retrieved = self.manager.get_session(session.session_id)
        self.assertIsNotNone(retrieved)
        self.assertEqual(retrieved.session_id, session.session_id)

    def test_get_nonexistent_session(self) -> None:
        """Тест получения несуществующей сессии."""
        result = self.manager.get_session("nonexistent")
        self.assertIsNone(result)

    def test_invalidate_session(self) -> None:
        """Тест инвалидации сессии."""
        session = self.manager.create_session(secrets.token_bytes(32))
        result = self.manager.invalidate_session(session.session_id)
        self.assertTrue(result)

        retrieved = self.manager.get_session(session.session_id)
        self.assertIsNone(retrieved)

    def test_session_eviction(self) -> None:
        """Тест вытеснения сессий при переполнении."""
        for i in range(7):
            self.manager.create_session(secrets.token_bytes(32))

        self.assertLessEqual(self.manager.active_sessions, 5)

    def test_session_expiration(self) -> None:
        """Тест истечения сессии."""
        manager = PQCSessionManager(max_sessions=5, ttl=0)
        session = manager.create_session(secrets.token_bytes(32))

        time.sleep(0.1)
        retrieved = manager.get_session(session.session_id)
        self.assertIsNone(retrieved)

    def test_cleanup_expired(self) -> None:
        """Тест очистки истекших сессий."""
        manager = PQCSessionManager(max_sessions=5, ttl=0)
        for _ in range(3):
            manager.create_session(secrets.token_bytes(32))

        time.sleep(0.1)
        removed = manager.cleanup_expired()
        self.assertEqual(removed, 3)
        self.assertEqual(manager.active_sessions, 0)


class TestOdysseyPQCTLS(TestCase):
    """Тесты Odyssey PQC TLS."""

    def setUp(self) -> None:
        """Настройка тестов."""
        self.config = OdysseyPQCConfig(
            mode=OdysseyPQCMode.HYBRID_X25519_ML_KEM,
            max_connections=10,
            enable_session_resumption=True,
        )
        self.odyssey = OdysseyPQCTLS(config=self.config)

    def test_initialization(self) -> None:
        """Тест инициализации."""
        self.assertIsNotNone(self.odyssey)
        self.assertEqual(
            self.odyssey._config.mode, OdysseyPQCMode.HYBRID_X25519_ML_KEM
        )

    def test_initialize_server(self) -> None:
        """Тест инициализации сервера."""
        server_keys = self.odyssey.initialize_server()

        self.assertIn("kem_public_key", server_keys)
        self.assertIn("sign_public_key", server_keys)
        self.assertIsInstance(server_keys["kem_public_key"], bytes)
        self.assertIsInstance(server_keys["sign_public_key"], bytes)

    def test_create_ssl_context_server(self) -> None:
        """Тест создания SSL контекста сервера."""
        context = self.odyssey.create_ssl_context(server_side=True)
        self.assertIsNotNone(context)
        self.assertEqual(context.protocol, ssl.PROTOCOL_TLS_SERVER)

    def test_create_ssl_context_client(self) -> None:
        """Тест создания SSL контекста клиента."""
        context = self.odyssey.create_ssl_context(server_side=False)
        self.assertIsNotNone(context)
        self.assertEqual(context.protocol, ssl.PROTOCOL_TLS_CLIENT)

    def test_perform_server_handshake(self) -> None:
        """Тест серверного рукопожатия."""
        self.odyssey.initialize_server()
        mock_socket = mock.MagicMock(spec=socket.socket)

        connection, shared_secret = self.odyssey.perform_server_handshake(
            mock_socket
        )

        self.assertIsNotNone(connection)
        self.assertEqual(connection.state, ConnectionState.READY)
        self.assertIsNotNone(shared_secret)
        self.assertIsInstance(shared_secret, bytes)

    def test_perform_client_handshake(self) -> None:
        """Тест клиентского рукопожатия."""
        server_keys = self.odyssey.initialize_server()
        mock_socket = mock.MagicMock(spec=socket.socket)

        connection, shared_secret = self.odyssey.perform_client_handshake(
            mock_socket, server_keys["kem_public_key"]
        )

        self.assertIsNotNone(connection)
        self.assertEqual(connection.state, ConnectionState.READY)
        self.assertIsNotNone(shared_secret)

    def test_get_connection(self) -> None:
        """Тест получения соединения."""
        mock_socket = mock.MagicMock(spec=socket.socket)
        connection, _ = self.odyssey.perform_server_handshake(mock_socket)

        retrieved = self.odyssey.get_connection(connection.conn_id)
        self.assertIsNotNone(retrieved)
        self.assertEqual(retrieved.conn_id, connection.conn_id)

    def test_release_connection(self) -> None:
        """Тест освобождения соединения."""
        mock_socket = mock.MagicMock(spec=socket.socket)
        connection, _ = self.odyssey.perform_server_handshake(mock_socket)

        connection.state = ConnectionState.IN_USE
        self.odyssey.release_connection(connection.conn_id)

        self.assertEqual(connection.state, ConnectionState.READY)

    def test_close_connection(self) -> None:
        """Тест закрытия соединения."""
        mock_socket = mock.MagicMock(spec=socket.socket)
        connection, _ = self.odyssey.perform_server_handshake(mock_socket)

        self.odyssey.close_connection(connection.conn_id)

        retrieved = self.odyssey.get_connection(connection.conn_id)
        self.assertIsNone(retrieved)

    def test_get_stats(self) -> None:
        """Тест получения статистики."""
        stats = self.odyssey.get_stats()

        self.assertIsInstance(stats, OdysseyStats)
        self.assertEqual(stats.total_connections, 0)
        self.assertEqual(stats.active_connections, 0)

    def test_generate_odyssey_config(self) -> None:
        """Тест генерации конфигурации Odyssey."""
        config = self.odyssey.generate_odyssey_config()

        self.assertIsInstance(config, str)
        self.assertIn("listeners", config)
        self.assertIn("pool", config)
        self.assertIn("authentication", config)
        self.assertIn("enable_pqc", config)

    def test_derive_session_keys(self) -> None:
        """Тест производства сессионных ключей."""
        shared_secret = secrets.token_bytes(32)
        keys = self.odyssey.derive_session_keys(shared_secret)

        self.assertIn("encryption", keys)
        self.assertIn("mac", keys)
        self.assertIn("iv", keys)
        self.assertIn("compression", keys)

        for key in keys.values():
            self.assertIsInstance(key, bytes)
            self.assertEqual(len(key), 32)


class TestOdysseyConnection(TestCase):
    """Тесты соединения Odyssey."""

    def test_connection_creation(self) -> None:
        """Тест создания соединения."""
        conn = OdysseyConnection(
            conn_id="test-conn-1",
            state=ConnectionState.IDLE,
        )

        self.assertEqual(conn.conn_id, "test-conn-1")
        self.assertEqual(conn.state, ConnectionState.IDLE)
        self.assertIsNone(conn.client_socket)
        self.assertIsNone(conn.server_socket)
        self.assertIsNone(conn.session)
        self.assertEqual(conn.bytes_sent, 0)
        self.assertEqual(conn.bytes_received, 0)
        self.assertEqual(conn.queries_count, 0)


class TestOdysseyStats(TestCase):
    """Тесты статистики Odyssey."""

    def test_default_stats(self) -> None:
        """Тест статистики по умолчанию."""
        stats = OdysseyStats()

        self.assertEqual(stats.total_connections, 0)
        self.assertEqual(stats.active_connections, 0)
        self.assertEqual(stats.pooled_connections, 0)
        self.assertEqual(stats.total_queries, 0)
        self.assertEqual(stats.pqc_handshakes, 0)
        self.assertEqual(stats.classical_handshakes, 0)
        self.assertEqual(stats.session_resumptions, 0)
        self.assertEqual(stats.avg_handshake_ms, 0.0)
        self.assertEqual(stats.avg_query_ms, 0.0)
        self.assertEqual(stats.bytes_sent, 0)
        self.assertEqual(stats.bytes_received, 0)
        self.assertEqual(stats.errors, 0)


class TestOdysseyPQCMode(TestCase):
    """Тесты режимов Odyssey PQC."""

    def test_pqc_modes(self) -> None:
        """Тест всех режимов PQC."""
        self.assertEqual(OdysseyPQCMode.PURE_ML_KEM.value, 1)
        self.assertEqual(OdysseyPQCMode.HYBRID_X25519_ML_KEM.value, 2)
        self.assertEqual(OdysseyPQCMode.CLASSICAL_FALLBACK.value, 3)


class TestConnectionState(TestCase):
    """Тесты состояний соединения."""

    def test_connection_states(self) -> None:
        """Тест всех состояний соединения."""
        self.assertEqual(ConnectionState.IDLE.value, 1)
        self.assertEqual(ConnectionState.CONNECTING.value, 2)
        self.assertEqual(ConnectionState.TLS_HANDSHAKE.value, 3)
        self.assertEqual(ConnectionState.PQC_HANDSHAKE.value, 4)
        self.assertEqual(ConnectionState.READY.value, 5)
        self.assertEqual(ConnectionState.IN_USE.value, 6)
        self.assertEqual(ConnectionState.CLOSING.value, 7)
        self.assertEqual(ConnectionState.CLOSED.value, 8)


if __name__ == "__main__":
    import unittest

    unittest.main()
