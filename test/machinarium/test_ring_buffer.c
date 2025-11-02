#include <machinarium.h>
#include <odyssey_test.h>

void machinarium_test_ring_buffer(void)
{
	machinarium_init();

	uint8_t buff[256];

	machine_ring_buffer_t *rbuf = machine_ring_buffer_create(10);

	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	test(machine_ring_buffer_read(rbuf, buff, 10) == 0);

	test(machine_ring_buffer_write(rbuf, "aaaaa", 5) == 5);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 5);
	test(machine_ring_buffer_free(rbuf) == 5);

	test(machine_ring_buffer_write(rbuf, "bbbbb", 4) == 4);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 9);
	test(machine_ring_buffer_free(rbuf) == 1);

	test(machine_ring_buffer_write(rbuf, "ccccc", 5) == 1);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 10);
	test(machine_ring_buffer_free(rbuf) == 0);

	test(machine_ring_buffer_write(rbuf, "ddddd", 5) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 10);
	test(machine_ring_buffer_free(rbuf) == 0);

	test(machine_ring_buffer_read(rbuf, buff, 5) == 5);
	test(memcmp(buff, "aaaaa", 5) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 5);
	test(machine_ring_buffer_free(rbuf) == 5);

	test(machine_ring_buffer_read(rbuf, buff + 5, 4) == 4);
	test(memcmp(buff, "aaaaabbbb", 9) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 1);
	test(machine_ring_buffer_free(rbuf) == 9);

	test(machine_ring_buffer_read(rbuf, buff + 9, 1) == 1);
	test(memcmp(buff, "aaaaabbbbc", 10) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	test(machine_ring_buffer_read(rbuf, buff, 5) == 0);
	test(memcmp(buff, "aaaaabbbbc", 10) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	test(machine_ring_buffer_write(rbuf, "eeeeeeeeeeeeeee", 15) == 10);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 10);
	test(machine_ring_buffer_free(rbuf) == 0);

	test(machine_ring_buffer_read(rbuf, buff, 10) == 10);
	test(memcmp(buff, "eeeeeeeeee", 10) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	test(machine_ring_buffer_write(rbuf, "ffffffff", 8) == 8);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 8);
	test(machine_ring_buffer_free(rbuf) == 2);

	test(machine_ring_buffer_read(rbuf, buff, 8) == 8);
	test(memcmp(buff, "ffffffff", 8) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	test(machine_ring_buffer_write(rbuf, "kkkkkkkk", 8) == 8);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 8);
	test(machine_ring_buffer_free(rbuf) == 2);

	test(machine_ring_buffer_read(rbuf, buff, 8) == 8);
	test(memcmp(buff, "kkkkkkkk", 8) == 0);
	test(machine_ring_buffer_capacity(rbuf) == 10);
	test(machine_ring_buffer_size(rbuf) == 0);
	test(machine_ring_buffer_free(rbuf) == 10);

	machine_ring_buffer_free(rbuf);

	machinarium_free();
}
