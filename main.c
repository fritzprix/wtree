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
	wtreeHeap_init(heap,sizeof(heap));
	rb_treeNode_t* buffer;
	root = NULL;
	size_t sz = 0;
	int test_cnt = 2;
	while(test_cnt--){
		int alloccnt = 100;
		while (alloccnt--) {
			sz = rand() % (sizeof(heap) >> 2);
			buffer = wtreeHeap_malloc(sz);
			if (buffer) {
				printf("inserted : addr -> %d , size -> %d\n ", buffer, sz);
				cdsl_rbtreeNodeInit(buffer, sz);
				cdsl_rbtreeInsert(&root, buffer);
			}
		}
		cdsl_rbtreePrint(&root);
		buffer = cdsl_rbtreeDeleteMax(&root);
		do {
			printf("chunk addr %d, chunk size : %d\n", (uint32_t) buffer,
					buffer->key);
			wtreeHeap_free(buffer);
			wtreeHeap_print();
			buffer = cdsl_rbtreeDeleteMax(&root);
		} while (buffer);
		cdsl_rbtreePrint(&root);
	}
	return 0;
}
