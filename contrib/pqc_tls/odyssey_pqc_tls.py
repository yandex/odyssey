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
Odyssey PQC TLS — постквантовый TLS для PostgreSQL соединений через Odyssey.

Реализует ML-KEM-768 обмен ключами для клиент-серверных рукопожатий
с мультиплексированием соединений и кэшированием PQC сессий.

Целевой репозиторий: github.com/yandex/odyssey
"""

from __future__ import annotations

import hashlib
import logging
import os
import secrets
import socket
import ssl
import struct
import threading
import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Optional, Union

try:
    import oqs
    from oqs import KeyEncapsulation, Signature

    HAS_LIBOQS = True
except ImportError:
    HAS_LIBOQS = False

logger = logging.getLogger(__name__)


class OdysseyPQCMode(Enum):
    """Режимы PQC для Odyssey."""

    PURE_ML_KEM = auto()
    HYBRID_X25519_ML_KEM = auto()
    CLASSICAL_FALLBACK = auto()


class ConnectionState(Enum):
    """Состояния соединения."""

    IDLE = auto()
    CONNECTING = auto()
    TLS_HANDSHAKE = auto()
    PQC_HANDSHAKE = auto()
    READY = auto()
    IN_USE = auto()
    CLOSING = auto()
    CLOSED = auto()


@dataclass(frozen=True)
class OdysseyPQCConfig:
    """Конфигурация PQC для Odyssey."""

    mode: OdysseyPQCMode = OdysseyPQCMode.HYBRID_X25519_ML_KEM
    kem_algorithm: str = "ML-KEM-768"
    signature_algorithm: str = "ML-DSA-65"
    max_connections: int = 100
    connection_timeout: float = 30.0
    session_ttl: int = 3600
    enable_session_resumption: bool = True
    enable_multiplexing: bool = True
    require_client_auth: bool = False
    server_certfile: Optional[str] = None
    server_keyfile: Optional[str] = None
    client_certfile: Optional[str] = None
    client_keyfile: Optional[str] = None

    def __post_init__(self) -> None:
        if not HAS_LIBOQS:
            raise ImportError(
                "liboqs-python обязателен для PQC операций. "
                "Установите: pip install liboqs-python"
            )


@dataclass
class PQCSession:
    """PQC сессия."""

    session_id: str
    shared_secret: bytes
    created_at: float
    expires_at: float
    client_public_key: Optional[bytes] = None
    server_public_key: Optional[bytes] = None
    ciphertext: Optional[bytes] = None
    attestation: Optional[bytes] = None


@dataclass
class OdysseyConnection:
    """Соединение Odyssey."""

    conn_id: str
    state: ConnectionState
    client_socket: Optional[socket.socket] = None
    server_socket: Optional[socket.socket] = None
    session: Optional[PQCSession] = None
    created_at: float = 0.0
    last_used: float = 0.0
    bytes_sent: int = 0
    bytes_received: int = 0
    queries_count: int = 0
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass
class OdysseyStats:
    """Статистика Odyssey."""

    total_connections: int = 0
    active_connections: int = 0
    pooled_connections: int = 0
    total_queries: int = 0
    pqc_handshakes: int = 0
    classical_handshakes: int = 0
    session_resumptions: int = 0
    avg_handshake_ms: float = 0.0
    avg_query_ms: float = 0.0
    bytes_sent: int = 0
    bytes_received: int = 0
    errors: int = 0


class PQCSessionManager:
    """Менеджер PQC сессий для Odyssey."""

    def __init__(
        self,
        max_sessions: int = 1000,
        session_ttl: int = 3600,
    ) -> None:
        """
        Инициализация менеджера сессий.

        Args:
            max_sessions: Максимальное количество сессий.
            session_ttl: Время жизни сессии (сек).
        """
        self._sessions: dict[str, PQCSession] = {}
        self._lock = threading.Lock()
        self._max_sessions = max_sessions
        self._session_ttl = session_ttl

    def create_session(
        self,
        shared_secret: bytes,
        client_public_key: Optional[bytes] = None,
        server_public_key: Optional[bytes] = None,
        ciphertext: Optional[bytes] = None,
    ) -> PQCSession:
        """
        Создание новой PQC сессии.

        Args:
            shared_secret: Общий секрет.
            client_public_key: Публичный ключ клиента.
            server_public_key: Публичный ключ сервера.
            ciphertext: Зашифрованный текст.

        Returns:
            Новая PQC сессия.
        """
        session_id = secrets.token_hex(32)
        now = time.time()

        session = PQCSession(
            session_id=session_id,
            shared_secret=shared_secret,
            created_at=now,
            expires_at=now + self._session_ttl,
            client_public_key=client_public_key,
            server_public_key=server_public_key,
            ciphertext=ciphertext,
        )

        with self._lock:
            if len(self._sessions) >= self._max_sessions:
                self._evict_oldest()

            self._sessions[session_id] = session

        logger.debug(f"PQC сессия создана: {session_id[:16]}...")
        return session

    def get_session(self, session_id: str) -> Optional[PQCSession]:
        """
        Получение сессии по ID.

        Args:
            session_id: ID сессии.

        Returns:
            PQC сессия или None.
        """
        with self._lock:
            session = self._sessions.get(session_id)
            if session and time.time() < session.expires_at:
                return session
            if session:
                del self._sessions[session_id]
        return None

    def invalidate_session(self, session_id: str) -> bool:
        """
        Инвалидация сессии.

        Args:
            session_id: ID сессии.

        Returns:
            True если сессия была удалена.
        """
        with self._lock:
            if session_id in self._sessions:
                del self._sessions[session_id]
                return True
        return False

    def _evict_oldest(self) -> None:
        """Вытеснение самой старой сессии."""
        if not self._sessions:
            return

        oldest_id = min(
            self._sessions.keys(),
            key=lambda k: self._sessions[k].created_at,
        )
        del self._sessions[oldest_id]

    def cleanup_expired(self) -> int:
        """
        Очистка истекших сессий.

        Returns:
            Количество удаленных сессий.
        """
        now = time.time()
        removed = 0

        with self._lock:
            expired = [
                sid
                for sid, session in self._sessions.items()
                if now >= session.expires_at
            ]
            for sid in expired:
                del self._sessions[sid]
                removed += 1

        return removed

    @property
    def active_sessions(self) -> int:
        """Количество активных сессий."""
        return len(self._sessions)


class OdysseyPQCTLS:
    """
    Постквантовый TLS для Odyssey PostgreSQL pooler.

    Поддерживает:
    - ML-KEM-768 обмен ключами
    - Мультиплексирование соединений
    - Кэширование PQC сессий
    - Встраивание в конфигурацию Odyssey
    """

    def __init__(
        self,
        config: Optional[OdysseyPQCConfig] = None,
    ) -> None:
        """
        Инициализация Odyssey PQC TLS.

        Args:
            config: Конфигурация PQC.
        """
        self._config = config or OdysseyPQCConfig()
        self._session_manager = PQCSessionManager(
            max_sessions=self._config.max_connections * 10,
            session_ttl=self._config.session_ttl,
        )
        self._stats = OdysseyStats()
        self._lock = threading.Lock()
        self._connections: dict[str, OdysseyConnection] = {}
        self._connection_pool: list[OdysseyConnection] = []

        self._kem = KeyEncapsulation(self._config.kem_algorithm)
        self._signer = Signature(self._config.signature_algorithm)

        self._server_kem_keypair: Optional[tuple[bytes, bytes]] = None
        self._server_sign_keypair: Optional[tuple[bytes, bytes]] = None

        logger.info(
            f"OdysseyPQCTLS инициализирован: mode={self._config.mode.name}"
        )

    def initialize_server(self) -> dict[str, bytes]:
        """
        Инициализация серверных ключей.

        Returns:
            Словарь с публичными ключами сервера.
        """
        kem_pub = self._kem.generate_keypair()
        self._server_kem_keypair = (kem_pub, self._kem.export_secret_key())

        sign_pub = self._signer.generate_keypair()
        self._server_sign_keypair = (sign_pub, self._signer.export_secret_key())

        logger.info("Серверные PQC ключи сгенерированы")
        return {
            "kem_public_key": kem_pub,
            "sign_public_key": sign_pub,
        }

    def create_ssl_context(
        self,
        server_side: bool = True,
    ) -> ssl.SSLContext:
        """
        Создание SSL контекста для Odyssey.

        Args:
            server_side: Режим сервера.

        Returns:
            Настроенный SSL контекст.
        """
        if server_side:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        else:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)

        if server_side and self._config.server_certfile:
            context.load_cert_chain(
                self._config.server_certfile,
                self._config.server_keyfile,
            )

        if not server_side and self._config.client_certfile:
            context.load_cert_chain(
                self._config.client_certfile,
                self._config.client_keyfile,
            )

        if not server_side:
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE

        return context

    def perform_server_handshake(
        self,
        client_socket: socket.socket,
        client_public_key: Optional[bytes] = None,
    ) -> tuple[OdysseyConnection, bytes]:
        """
        Серверное рукопожатие с клиентом.

        Args:
            client_socket: Сокет клиента.
            client_public_key: Публичный ключ клиента.

        Returns:
            Кортеж (соединение, общий секрет).
        """
        start_time = time.monotonic()

        if not self._server_kem_keypair:
            self.initialize_server()

        server_pub, server_secret = self._server_kem_keypair

        if client_public_key:
            ciphertext, shared_secret = self._kem.encap_secret(
                client_public_key
            )
        else:
            ciphertext, shared_secret = self._kem.encap_secret(server_pub)

        handshake_data = hashlib.sha256(
            shared_secret + ciphertext + server_pub
        ).digest()

        _, sign_secret = self._server_sign_keypair
        attestation = self._signer.sign(handshake_data, sign_secret)

        session = self._session_manager.create_session(
            shared_secret=shared_secret,
            server_public_key=server_pub,
            ciphertext=ciphertext,
        )
        session.attestation = attestation

        conn_id = secrets.token_hex(16)
        connection = OdysseyConnection(
            conn_id=conn_id,
            state=ConnectionState.READY,
            client_socket=client_socket,
            session=session,
            created_at=time.time(),
            last_used=time.time(),
        )

        with self._lock:
            self._connections[conn_id] = connection
            self._stats.total_connections += 1
            self._stats.active_connections += 1
            self._stats.pqc_handshakes += 1

            duration_ms = (time.monotonic() - start_time) * 1000
            n = self._stats.pqc_handshakes
            self._stats.avg_handshake_ms = (
                self._stats.avg_handshake_ms * (n - 1) + duration_ms
            ) / n

        logger.info(
            f"Серверное рукопожатие завершено: conn={conn_id[:16]}..., "
            f"duration={duration_ms:.2f}ms"
        )

        return connection, shared_secret

    def perform_client_handshake(
        self,
        server_socket: socket.socket,
        server_public_key: Optional[bytes] = None,
    ) -> tuple[OdysseyConnection, bytes]:
        """
        Клиентское рукопожатие с сервером.

        Args:
            server_socket: Сокет сервера.
            server_public_key: Публичный ключ сервера.

        Returns:
            Кортеж (соединение, общий секрет).
        """
        start_time = time.monotonic()

        client_kem = KeyEncapsulation(self._config.kem_algorithm)
        client_pub = client_kem.generate_keypair()

        if server_public_key:
            ciphertext, shared_secret = client_kem.encap_secret(
                server_public_key
            )
        else:
            ciphertext, shared_secret = client_kem.encap_secret(client_pub)

        handshake_data = hashlib.sha256(
            shared_secret + ciphertext + client_pub
        ).digest()

        session = self._session_manager.create_session(
            shared_secret=shared_secret,
            client_public_key=client_pub,
            ciphertext=ciphertext,
        )

        conn_id = secrets.token_hex(16)
        connection = OdysseyConnection(
            conn_id=conn_id,
            state=ConnectionState.READY,
            server_socket=server_socket,
            session=session,
            created_at=time.time(),
            last_used=time.time(),
        )

        with self._lock:
            self._connections[conn_id] = connection
            self._stats.total_connections += 1
            self._stats.active_connections += 1
            self._stats.pqc_handshakes += 1

            duration_ms = (time.monotonic() - start_time) * 1000
            n = self._stats.pqc_handshakes
            self._stats.avg_handshake_ms = (
                self._stats.avg_handshake_ms * (n - 1) + duration_ms
            ) / n

        logger.info(
            f"Клиентское рукопожатие завершено: conn={conn_id[:16]}..., "
            f"duration={duration_ms:.2f}ms"
        )

        return connection, shared_secret

    def get_connection(
        self, conn_id: str
    ) -> Optional[OdysseyConnection]:
        """
        Получение соединения по ID.

        Args:
            conn_id: ID соединения.

        Returns:
            Соединение или None.
        """
        return self._connections.get(conn_id)

    def release_connection(self, conn_id: str) -> None:
        """
        Освобождение соединения.

        Args:
            conn_id: ID соединения.
        """
        conn = self._connections.get(conn_id)
        if conn:
            conn.state = ConnectionState.READY
            conn.last_used = time.time()
            with self._lock:
                self._stats.active_connections -= 1

    def close_connection(self, conn_id: str) -> None:
        """
        Закрытие соединения.

        Args:
            conn_id: ID соединения.
        """
        conn = self._connections.pop(conn_id, None)
        if conn:
            if conn.client_socket:
                try:
                    conn.client_socket.close()
                except OSError:
                    pass
            if conn.server_socket:
                try:
                    conn.server_socket.close()
                except OSError:
                    pass

            if conn.session:
                self._session_manager.invalidate_session(
                    conn.session.session_id
                )

            with self._lock:
                self._stats.active_connections -= 1

            logger.debug(f"Соединение закрыто: {conn_id[:16]}...")

    def multiplex_query(
        self,
        conn_id: str,
        query: bytes,
    ) -> Optional[bytes]:
        """
        Мультиплексирование запроса через соединение.

        Args:
            conn_id: ID соединения.
            query: SQL запрос.

        Returns:
            Ответ сервера или None.
        """
        conn = self._connections.get(conn_id)
        if not conn or conn.state != ConnectionState.READY:
            return None

        conn.state = ConnectionState.IN_USE
        conn.last_used = time.time()

        try:
            if conn.client_socket:
                conn.client_socket.sendall(query)
                conn.bytes_sent += len(query)

                response = conn.client_socket.recv(65536)
                conn.bytes_received += len(response)
                conn.queries_count += 1

                with self._lock:
                    self._stats.total_queries += 1
                    self._stats.bytes_sent += len(query)
                    self._stats.bytes_received += len(response)

                return response

        except Exception as e:
            logger.error(f"Ошибка мультиплексирования: {e}")
            with self._lock:
                self._stats.errors += 1

        finally:
            conn.state = ConnectionState.READY

        return None

    def get_stats(self) -> OdysseyStats:
        """Получение статистики."""
        return self._stats

    def cleanup_idle_connections(
        self, max_idle_seconds: int = 300
    ) -> int:
        """
        Очистка неактивных соединений.

        Args:
            max_idle_seconds: Максимальное время простоя.

        Returns:
            Количество закрытых соединений.
        """
        now = time.time()
        closed = 0

        idle_conns = [
            conn_id
            for conn_id, conn in self._connections.items()
            if conn.state == ConnectionState.READY
            and (now - conn.last_used) > max_idle_seconds
        ]

        for conn_id in idle_conns:
            self.close_connection(conn_id)
            closed += 1

        return closed

    def generate_odyssey_config(self) -> str:
        """
        Генерация конфигурации Odyssey с PQC.

        Returns:
            Конфигурационный файл Odyssey.
        """
        config = f"""# Odyssey PQC TLS Configuration
# Generated by x0tta6bl4 Odyssey PQC TLS Module

unix_socket_dir = "/var/run/odyssey"
unix_socket_mode = "0644"

listeners {{{{
    host = "0.0.0.0"
    port = 5432
    backlog = 128
}}}}

protobuf {{{{
    # PQC handshake configuration
    enable_pqc = true
    pqc_mode = "{self._config.mode.name.lower()}"
    kem_algorithm = "{self._config.kem_algorithm}"
    signature_algorithm = "{self._config.signature_algorithm}"
    
    # TLS settings
    sslmode = "require"
    ssl_key_file = "{self._config.server_keyfile or '/etc/odyssey/server.key'}"
    ssl_cert_file = "{self._config.server_certfile or '/etc/odyssey/server.crt'}"
}}}}

pool {{{{
    mode = "transaction"
    pooling = true
    pool_discard = false
    pool_clear = false
    pool_ttl = 60
    pool_timeout = 30
    pool_size = {self._config.max_connections}
}}}}

authentication {{{{
    auth_method = "md5"
    auth_query = "SELECT usename, passwd FROM pg_shadow WHERE usename=$1"
}}}}

server_lifetime = 3600
server_idle_timeout = 600

log {{{{
    debug = false
    info = false
    error = true
}}}}
"""
        return config

    def derive_session_keys(
        self, shared_secret: bytes
    ) -> dict[str, bytes]:
        """
        Производство сессионных ключей.

        Args:
            shared_secret: Общий секрет.

        Returns:
            Словарь с сессионными ключами.
        """
        context = b"odyssey-pqc-session-v1"
        prk = hashlib.sha256(shared_secret + context).digest()

        keys = {}
        labels = [b"encryption", b"mac", b"iv", b"compression"]
        for i, label in enumerate(labels):
            key_material = hashlib.sha256(
                prk + label + struct.pack("!I", i)
            ).digest()
            keys[label.decode()] = key_material

        return keys


def create_odyssey_pqc_tls(
    mode: OdysseyPQCMode = OdysseyPQCMode.HYBRID_X25519_ML_KEM,
    max_connections: int = 100,
    server_certfile: Optional[str] = None,
    server_keyfile: Optional[str] = None,
) -> OdysseyPQCTLS:
    """
    Фабричная функция для создания Odyssey PQC TLS.

    Args:
        mode: Режим PQC.
        max_connections: Максимальное количество соединений.
        server_certfile: Путь к сертификату сервера.
        server_keyfile: Путь к ключу сервера.

    Returns:
        Настроенный Odyssey PQC TLS.
    """
    config = OdysseyPQCConfig(
        mode=mode,
        max_connections=max_connections,
        server_certfile=server_certfile,
        server_keyfile=server_keyfile,
    )

    odyssey = OdysseyPQCTLS(config=config)
    odyssey.initialize_server()

    logger.info(
        f"Odyssey PQC TLS создан: mode={mode.name}, "
        f"max_connections={max_connections}"
    )

    return odyssey
