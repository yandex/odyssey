/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <soft_oom.h>
#include <tests/odyssey_test.h>

static void check(char *meminfo, int64_t expected_mem_available,
		  int64_t expected_mem_total)
{
	int64_t mem_available, mem_total;

	od_soft_oom_get_system_memory_consumption_from_meminfo(
		meminfo, &mem_available, &mem_total);

	test(mem_available == expected_mem_available);
	test(mem_total == expected_mem_total);
}

static void test_usual(void)
{
	char meminfo[] = "MemTotal:        6052628 kB\n"
			 "MemFree:         4977920 kB\n"
			 "MemAvailable:    5704804 kB\n"
			 "Buffers:           63688 kB\n"
			 "Cached:           789024 kB\n"
			 "SwapCached:            0 kB\n"
			 "Active:           654288 kB\n"
			 "Inactive:         294288 kB\n"
			 "Active(anon):      96612 kB\n"
			 "Inactive(anon):        0 kB\n"
			 "Active(file):     557676 kB\n"
			 "Inactive(file):   294288 kB\n"
			 "Unevictable:           0 kB\n"
			 "Mlocked:               0 kB\n"
			 "SwapTotal:             0 kB\n"
			 "SwapFree:              0 kB\n"
			 "Zswap:                 0 kB\n"
			 "Zswapped:              0 kB\n"
			 "Dirty:                12 kB\n"
			 "Writeback:             0 kB\n"
			 "AnonPages:         95880 kB\n"
			 "Mapped:           152956 kB\n"
			 "Shmem:               732 kB\n"
			 "KReclaimable:      27124 kB\n"
			 "Slab:              72036 kB\n"
			 "SReclaimable:      27124 kB\n"
			 "SUnreclaim:        44912 kB\n"
			 "KernelStack:        2300 kB\n"
			 "ShadowCallStack:     576 kB\n"
			 "PageTables:         2412 kB\n"
			 "SecPageTables:         0 kB\n"
			 "NFS_Unstable:          0 kB\n"
			 "Bounce:                0 kB\n"
			 "WritebackTmp:          0 kB\n"
			 "CommitLimit:     3026312 kB\n"
			 "Committed_AS:     778660 kB\n"
			 "VmallocTotal:   133141626880 kB\n"
			 "VmallocUsed:        7564 kB\n"
			 "VmallocChunk:          0 kB\n"
			 "Percpu:              832 kB\n"
			 "HardwareCorrupted:     0 kB\n"
			 "AnonHugePages:         0 kB\n"
			 "ShmemHugePages:        0 kB\n"
			 "ShmemPmdMapped:        0 kB\n"
			 "FileHugePages:         0 kB\n"
			 "FilePmdMapped:         0 kB\n"
			 "CmaTotal:          32768 kB\n"
			 "CmaFree:           29696 kB\n"
			 "HugePages_Total:       0\n"
			 "HugePages_Free:        0\n"
			 "HugePages_Rsvd:        0\n"
			 "HugePages_Surp:        0\n"
			 "Hugepagesize:       2048 kB\n"
			 "Hugetlb:               0 kB\n";

	check(meminfo, 5841719296LL, 6197891072LL);
}

static void test_equality(void)
{
	char meminfo[] = "MemTotal:        366820906 kB\n"
			 "MemFree:         143430131 kB\n"
			 "MemAvailable:    366820906 kB\n";

	int64_t bytes = 375624607744LL;

	check(meminfo, bytes, bytes);
}

static void test_available_more_total(void)
{
	char meminfo[] = "MemTotal:        246332760 kB\n"
			 "MemFree:         366820906 kB\n"
			 "MemAvailable:    629889926 kB\n";

	check(meminfo, 645007284224LL, 252244746240LL);
}

static void test_empty(void)
{
	char meminfo[] = "";

	check(meminfo, 0, 0);
}

static void test_large_values(void)
{
	char meminfo[] = "MemTotal:        772924930726 kB\n"
			 "MemFree:         471125620524 kB\n"
			 "MemAvailable:    489460279738 kB\n";

	check(meminfo, 501207326451712LL, 791475129063424LL);
}

void odyssey_test_soft_oom(void)
{
	test_usual();
	test_equality();
	test_available_more_total();
	test_empty();
	test_large_values();
}
