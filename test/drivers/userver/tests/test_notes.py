import pytest


@pytest.mark.asyncio
async def test_create_and_get_note(service_client):
    async with service_client.post(
        '/v1/notes',
        json={'text': 'hello via odyssey'},
    ) as response:
        assert response.status == 200
        created = await response.json()

    assert 'id' in created
    assert created['text'] == 'hello via odyssey'

    note_id = created['id']

    async with service_client.get(f'/v1/notes/{note_id}') as response:
        assert response.status == 200
        fetched = await response.json()

    assert fetched == {
        'id': note_id,
        'text': 'hello via odyssey',
    }
