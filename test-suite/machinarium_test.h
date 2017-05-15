#ifndef MACHINARIUM_TEST_H
#define MACHINARIUM_TEST_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#define machinarium_test(function) \
	do { \
		(function)(); \
		fprintf(stdout, "%s: ok\n", #function); \
	} while (0);

#define test(expression) \
	do { \
		if (! (expression)) { \
			fprintf(stdout, "%s: fail (%s:%d) %s\n", \
			        __func__, __FILE__, __LINE__, #expression); \
			fflush(stdout); \
			abort(); \
		} \
	} while (0);

#endif
