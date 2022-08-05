
#include "odyssey.h"
#include <odyssey_test.h>

void test_od_hba_reader_prefix()
{
	od_hba_rule_t *hba = NULL;
	char buffer[INET_ADDRSTRLEN];

	char *prefix = "24";
	char *value = "255.255.255.0";
	hba = od_hba_rule_create();
	hba->addr.ss_family = AF_INET;
	test(od_hba_reader_prefix(hba, prefix) == 0);
	struct sockaddr_in *addr = (struct sockaddr_in *)&hba->mask;
	inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
	test(memcmp(value, buffer, strlen(buffer)) == 0);

	prefix = "12";
	value = "255.240.0.0";
	hba = od_hba_rule_create();
	hba->addr.ss_family = AF_INET;
	test(od_hba_reader_prefix(hba, prefix) == 0);
	addr = (struct sockaddr_in *)&hba->mask;
	inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
	test(memcmp(value, buffer, strlen(buffer)) == 0);
}

void odyssey_test_hba(void)
{
	test_od_hba_reader_prefix();
}