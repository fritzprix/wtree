/*
 * bstree.c
 *
 *  Created on: 2016. 12. 7.
 *      Author: innocentevil
 */


#include "bstree.h"

#define DIRECTION_LEFT    ((uint8_t) 1)
#define DIRECTION_RIGHT   ((uint8_t) 2)


static bsNode* bs_insert_rc(bsNode* parent, bsNode* node, bs_traverse_callback callback);
static bsNode* bs_traverse_rc(bsNode* parent, bsNode* node, bs_traverse_callback callback);
static bsNode* bs_remove_rc(bsNode* parent, bsNode* node);
static bsNode* bs_popup_leaf_rc(bsNode* parent, int direction);

void bs_rootInit(bsRoot* root) {
	if(!root) {
		return;
	}
	root->entry = NULL;
}

void bs_nodeInit(bsNode* node) {
	if(!node) {
		return;
	}
	node->left = node->right = NULL;
}

void bs_insert(bsRoot* root, bsNode* node, bs_traverse_callback callback) {
	if(!root || !node) {
		return;
	}
	root->entry = bs_insert_rc(root->entry, node, callback);
}

void bs_traverse(bsRoot* root, bsNode* target, bs_traverse_callback callback) {
	if(!root) {
		// Null target is intentionally allowed to traverse to minimum node of the tree
		return;
	}
	bs_traverse_rc(root->entry, target, callback);
}

void bs_remove(bsRoot* root, bsNode* node) {
	if(!root || !node) {
		return;
	}
	root->entry = bs_remove_rc(root->entry, node);
}

static bsNode* bs_traverse_rc(bsNode* parent, bsNode* target, bs_traverse_callback callback) {
	if(!parent) {
		// Null target is intentionally allowed to traverse to minimum node of the tree
		return NULL;
	}
	switch(callback(parent)) {
	case BS_REMOVE_THIS:
		bsNode* alt = NULL;
		if(parent->left) {
			alt = bs_popup_leaf_rc(parent->left, DIRECTION_RIGHT);
			if(alt != parent->left) {
				alt->left = parent->left;
			}
			alt->right = parent->right;
		} else if(parent->right) {
			alt = bs_popup_leaf_rc(parent->right, DIRECTION_LEFT);
			if(alt != parent->right) {
				alt->right = parent->right;
			}
			alt->left = parent->left;
		}
		return alt;
	case BS_KEEP:
		if(((size_t) parent) > ((size_t) target)) {
			parent->left = bs_traverse_rc(parent->left, target, callback);
		} else if(((size_t) parent) < ((size_t) target)){
			parent->right = bs_traverse_rc(parent->right, target, callback);
		}
		break;
	case BS_STOP:
		// NOTHING TO DO HERE
		break;
	}
	return parent;
}

static bsNode* bs_remove_rc(bsNode* parent, bsNode* node) {

}



static bsNode* bs_insert_rc(bsNode* parent, bsNode* node, bs_traverse_callback callback) {
	if(!parent) {
		return node;
	}
	switch(callback(parent)) {
	case BS_REMOVE_THIS:
		bsNode* alt = NULL;
		if(parent->left) {
			alt = bs_popup_leaf_rc(parent->left, DIRECTION_RIGHT);
			if(alt != parent->left) {
				alt->left = parent->left;
			}
			alt->right = parent->right;
		} else if(parent->right) {
			alt = bs_popup_leaf_rc(parent->right, DIRECTION_LEFT);
			if(alt != parent->right) {
				alt->right = parent->right;
			}
			alt->left = parent->left;
		}
		return alt;
	case BS_KEEP:
		if(((size_t) parent) > ((size_t) node)) {
			parent->left = bs_insert_rc(parent->left, node, callback);
		} else if(((size_t) parent) < ((size_t) node)){
			parent->right = bs_insert_rc(parent->right, node, callback);
		}
		break;
	case BS_STOP:
		// NOTHING TO DO HERE
		break;
	}
	return parent;
}

static bsNode* bs_popup_leaf_rc(bsNode* parent, int direction) {
	if(!parent) {
		return NULL;
	}
	bsNode* leaf = NULL;
	switch(direction) {
	case DIRECTION_LEFT:
		if(!parent->left) {
			return parent;
		}
		leaf = bs_popup_leaf_rc(parent->left, direction);
		if(!leaf) {
			leaf = parent->left;
			parent->left = NULL;
		}
		break;
	case DIRECTION_RIGHT:
		if(!parent->right) {
			return parent;
		}
		leaf = bs_popup_leaf_rc(parent->right, direction);
		if(!leaf) {
			leaf = parent->right;
			parent->right = NULL;
		}
		break;
	}
	return leaf;
}


