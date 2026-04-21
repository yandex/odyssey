import aiohttp
import pytest_asyncio


@pytest_asyncio.fixture
async def service_client():
    async with aiohttp.ClientSession(
        base_url='http://127.0.0.1:8080',
    ) as session:
        yield session
