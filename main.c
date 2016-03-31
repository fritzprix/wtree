/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "wtree.h"
#include "wtalloc.h"
#include "ymalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "cdsl_nrbtree.h"


typedef struct  {
	nrbtreeNode_t node;
	char          name[20];
	int           age;
}person_t;

int main(void){
	int cnt = 1000;
	while(cnt--){
		person_t* p = ymalloc(sizeof(person_t));
		printf("allocated address is : %lx\n",(size_t) p);
		p->age = 36;
		cdsl_nrbtreeNodeInit(&p->node,36);
		sprintf(p->name, "DooWoong");
	}
	printf("exit normal\n");
	return 0;
}
