import pytest


@pytest.mark.asyncio
@pytest.mark.parametrize('chunk_size', [1, 7, 64, 128, 1024])
async def test_portal_scan_validates_rows(service_client, chunk_size):
    rows_count = 50000

    async with service_client.post(
        f'/v1/portal-fill?count={rows_count}',
    ) as response:
        assert response.status == 200
        filled = await response.json(content_type=None)

    assert filled['inserted'] == rows_count

    async with service_client.get(
        f'/v1/portal-scan?chunk_size={chunk_size}&last_id=0',
    ) as response:
        assert response.status == 200
        scanned = await response.json(content_type=None)

    assert scanned['ok'] is True
    assert scanned['rows'] == rows_count
    assert scanned['last_id'] == rows_count
    assert scanned['chunk_size'] == chunk_size

@pytest.mark.asyncio
async def test_portal_scan_validates_tail(service_client):
    rows_count = 50000
    last_id = 3456
    expected_rows = rows_count - last_id

    async with service_client.post(
        f'/v1/portal-fill?count={rows_count}',
    ) as response:
        assert response.status == 200

    async with service_client.get(
        f'/v1/portal-scan?chunk_size=113&last_id={last_id}',
    ) as response:
        assert response.status == 200
        scanned = await response.json(content_type=None)

    assert scanned['ok'] is True
    assert scanned['rows'] == expected_rows
    assert scanned['last_id'] == rows_count
