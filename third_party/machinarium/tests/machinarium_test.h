#ifndef MACHINARIUM_TEST_H
#define MACHINARIUM_TEST_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#define machinarium_test(function) \
	do { \
		fprintf(stdout, "%s: ", #function); \
		fflush(stdout); \
		(function)(); \
		fprintf(stdout, "ok\n"); \
	} while (0);

#define test(expression) \
	do { \
		if (! (expression)) { \
			fprintf(stdout, "fail (%s:%d) %s\n", \
			        __FILE__, __LINE__, #expression); \
			fflush(stdout); \
			abort(); \
		} \
	} while (0);

#endif
