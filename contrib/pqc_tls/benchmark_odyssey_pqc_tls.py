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
Бенчмарк для Odyssey PQC TLS.

Замеряет время рукопожатия, производительность запросов и overhead PQC.
"""

import secrets
import statistics
import time
from typing import Optional

from src.integration.yandex.odyssey_pqc_tls import (
    OdysseyPQCConfig,
    OdysseyPQCMode,
    OdysseyPQCTLS,
    PQCSessionManager,
)


def benchmark_handshake(
    iterations: int = 100,
) -> dict[str, float]:
    """Бенчмарк рукопожатия."""
    config = OdysseyPQCConfig(
        mode=OdysseyPQCMode.HYBRID_X25519_ML_KEM,
        enable_session_resumption=True,
    )

    server = OdysseyPQCTLS(config=config)
    server.initialize_server()

    server_keys = server._server_kem_keypair

    handshake_times = []

    for _ in range(iterations):
        start = time.perf_counter()

        client = OdysseyPQCTLS(config=config)
        _, _ = client.perform_client_handshake(
            mock_socket=None,
            server_public_key=server_keys[0] if server_keys else None,
        )

        handshake_times.append(time.perf_counter() - start)

    return {
        "handshake_avg_ms": statistics.mean(handshake_times) * 1000,
        "handshake_p95_ms": sorted(handshake_times)[int(0.95 * len(handshake_times))] * 1000,
        "handshake_p99_ms": sorted(handshake_times)[int(0.99 * len(handshake_times))] * 1000,
    }


def benchmark_session_management(
    iterations: int = 10000,
) -> dict[str, float]:
    """Бенчмарк управления сессиями."""
    manager = PQCSessionManager(max_sessions=1000, ttl=3600)

    create_times = []
    get_times = []
    invalidate_times = []

    session_ids = []

    for i in range(iterations):
        start = time.perf_counter()
        session = manager.create_session(secrets.token_bytes(32))
        create_times.append(time.perf_counter() - start)
        session_ids.append(session.session_id)

    for session_id in session_ids[:iterations // 2]:
        start = time.perf_counter()
        manager.get_session(session_id)
        get_times.append(time.perf_counter() - start)

    for session_id in session_ids[iterations // 4:iterations // 2]:
        start = time.perf_counter()
        manager.invalidate_session(session_id)
        invalidate_times.append(time.perf_counter() - start)

    return {
        "create_session_avg_us": statistics.mean(create_times) * 1_000_000,
        "create_session_p95_us": sorted(create_times)[int(0.95 * len(create_times))] * 1_000_000,
        "get_session_avg_us": statistics.mean(get_times) * 1_000_000,
        "get_session_p95_us": sorted(get_times)[int(0.95 * len(get_times))] * 1_000_000,
        "invalidate_session_avg_us": statistics.mean(invalidate_times) * 1_000_000,
        "invalidate_session_p95_us": sorted(invalidate_times)[int(0.95 * len(invalidate_times))] * 1_000_000,
    }


def benchmark_key_derivation(
    iterations: int = 10000,
) -> dict[str, float]:
    """Бенчмарк производства ключей."""
    config = OdysseyPQCConfig(mode=OdysseyPQCMode.HYBRID_X25519_ML_KEM)
    odyssey = OdysseyPQCTLS(config=config)

    shared_secret = secrets.token_bytes(32)

    derivation_times = []

    for _ in range(iterations):
        start = time.perf_counter()
        keys = odyssey.derive_session_keys(shared_secret)
        derivation_times.append(time.perf_counter() - start)

    return {
        "key_derivation_avg_us": statistics.mean(derivation_times) * 1_000_000,
        "key_derivation_p95_us": sorted(derivation_times)[int(0.95 * len(derivation_times))] * 1_000_000,
    }


def benchmark_config_generation(
    iterations: int = 1000,
) -> dict[str, float]:
    """Бенчмарк генерации конфигурации."""
    config = OdysseyPQCConfig(mode=OdysseyPQCMode.HYBRID_X25519_ML_KEM)
    odyssey = OdysseyPQCTLS(config=config)

    generation_times = []

    for _ in range(iterations):
        start = time.perf_counter()
        _ = odyssey.generate_odyssey_config()
        generation_times.append(time.perf_counter() - start)

    return {
        "config_generation_avg_ms": statistics.mean(generation_times) * 1000,
        "config_generation_p95_ms": sorted(generation_times)[int(0.95 * len(generation_times))] * 1000,
    }


def benchmark_session_cleanup(
    iterations: int = 100,
) -> dict[str, float]:
    """Бенчмарк очистки сессий."""
    cleanup_times = []

    for _ in range(iterations):
        manager = PQCSessionManager(max_sessions=1000, ttl=0)

        for i in range(100):
            manager.create_session(secrets.token_bytes(32))

        start = time.perf_counter()
        manager.cleanup_expired()
        cleanup_times.append(time.perf_counter() - start)

    return {
        "cleanup_100_sessions_avg_ms": statistics.mean(cleanup_times) * 1000,
        "cleanup_100_sessions_p95_ms": sorted(cleanup_times)[int(0.95 * len(cleanup_times))] * 1000,
    }


def benchmark_memory_usage() -> dict[str, int]:
    """Замер использования памяти."""
    import sys

    config = OdysseyPQCConfig(mode=OdysseyPQCMode.HYBRID_X25519_ML_KEM)
    odyssey = OdysseyPQCTLS(config=config)

    odyssey.initialize_server()

    session_manager = PQCSessionManager(max_sessions=10000, ttl=3600)

    for i in range(1000):
        session_manager.create_session(secrets.token_bytes(32))

    return {
        "session_manager_sessions": session_manager.active_sessions,
        "session_manager_bytes_estimate": session_manager.active_sessions * 200,
        "odyssey_connections": len(odyssey._connections),
    }


def run_all_benchmarks() -> None:
    """Запуск всех бенчмарков."""
    print("=" * 60)
    print("Odyssey PQC TLS Benchmarks")
    print("=" * 60)

    print("\n1. Handshake Performance:")
    handshake_results = benchmark_handshake()
    for key, value in handshake_results.items():
        print(f"   {key}: {value:.3f}ms")

    print("\n2. Session Management:")
    session_results = benchmark_session_management()
    for key, value in session_results.items():
        print(f"   {key}: {value:.3f}us")

    print("\n3. Key Derivation:")
    key_results = benchmark_key_derivation()
    for key, value in key_results.items():
        print(f"   {key}: {value:.3f}us")

    print("\n4. Config Generation:")
    config_results = benchmark_config_generation()
    for key, value in config_results.items():
        print(f"   {key}: {value:.3f}ms")

    print("\n5. Session Cleanup:")
    cleanup_results = benchmark_session_cleanup()
    for key, value in cleanup_results.items():
        print(f"   {key}: {value:.3f}ms")

    print("\n6. Memory Usage:")
    memory_results = benchmark_memory_usage()
    for key, value in memory_results.items():
        print(f"   {key}: {value}")

    print("\n" + "=" * 60)
    print("Benchmarks completed")
    print("=" * 60)


if __name__ == "__main__":
    run_all_benchmarks()
