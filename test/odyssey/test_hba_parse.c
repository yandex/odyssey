
#include "odyssey.h"
#include <odyssey_test.h>

void test_od_hba_reader_prefix(char *prefix, char *value)
{
	od_hba_rule_t *hba = NULL;
	char buffer[INET_ADDRSTRLEN];
	hba = od_hba_rule_create();
	hba->addr.ss_family = AF_INET;
	test(od_hba_reader_prefix(hba, prefix) == 0);
	struct sockaddr_in *addr = (struct sockaddr_in *)&hba->mask;
	inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
	test(memcmp(value, buffer, strlen(buffer)) == 0);
	od_hba_rule_free(hba);
}

void odyssey_test_hba(void)
{
	test_od_hba_reader_prefix("31", "255.255.255.254");
	test_od_hba_reader_prefix("24", "255.255.255.0");
	test_od_hba_reader_prefix("12", "255.240.0.0");
	test_od_hba_reader_prefix("7", "254.0.0.0");
}