/*
 * main.c
 *
 *  Created on: Jun 25, 2016
 *      Author: innocentevil
 */


#include "wtree.h"
#include "segment.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>

static DECLARE_MAPPER(mapper);
static DECLARE_UNMAPPER(unmapper);


int main(void) {
	segmentRoot_t segroot;
	segment_root_init(&segroot, mapper, unmapper);

	return 0;
}


static DECLARE_MAPPER(mapper) {

}

static DECLARE_UNMAPPER(unmapper) {

}
