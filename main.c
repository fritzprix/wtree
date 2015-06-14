/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "wtree.h"
#include "malloc.h"
#include <stdio.h>
#include "cdsl_rbtree.h"

static uint8_t heap[1 << 16];
static rb_treeNode_t* root;

int main(void){
	int tcnt = 100;
	wtreeHeap_init(heap,sizeof(heap));
	rb_treeNode_t* buffer;
	root = NULL;
	size_t sz = 0;
	while(tcnt--){
		sz = rand() % (sizeof(heap) >> 2);
		buffer = wtreeHeap_malloc(sz);
		if(buffer){
			cdsl_rbtreeNodeInit(buffer,sz);
			cdsl_rbtreeInsert(&root,buffer);
		}
		printf("Loop\n");
	}
	cdsl_rbtreePrint(&root);
	do {
		buffer = cdsl_rbtreeDeleteMax(&root);
		wtreeHeap_free(buffer);
		wtreeHeap_print();
	}while(buffer);
	cdsl_rbtreePrint(&root);
	return 0;
}
