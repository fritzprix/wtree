/*
 * main.c
 *
 *  Created on: 2016. 12. 19.
 *      Author: innocentevil
 */

#include "common.h"
#include "bstree_test.h"
#include "assert.h"

int main(void) {
	assert(utest_bstree() == 0);
#if (BENCHMARK_ENABLE > 0)
	perform_benchmark();
#endif
	PRINT("TEST Successful\n");
	return 0;
}
