/* -*- c -*- */

/*
 * avl.h
 *
 * Copyright (C) 1994-2004 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __AVL_H__
#define __AVL_H__

#include <limits.h>

#define INT_BIT_M2 (sizeof(int) * CHAR_BIT - 2)

typedef int (*avlt_compare_func_t) (void *a, void *b, void *params);

typedef struct _avltnode_t avltnode_t;
struct _avltnode_t
{
    void *data;
    int balance : 2;		/* one of -1, 0, 1 */
    unsigned int rank : INT_BIT_M2;
    avltnode_t *up;
    avltnode_t *left;
    avltnode_t *right;
};

typedef struct _avltree_t avltree_t;
struct _avltree_t
{
    avltnode_t *head;
    unsigned int num_elements;
    avlt_compare_func_t compare;
};

/*
 * The elements of the tree are numbered beginning with 1. I
 * should change this.
 */

/*
 * Creates a new unsorted tree, which can be used like a list.
 */
avltree_t* avlt_create_unsorted (void);

/*
 * Creates a new sorted tree.
 */
avltree_t* avlt_create_sorted (avlt_compare_func_t compare);

void avlt_destroy (avltree_t *tree);

/*
 * Copies a tree.
 */
avltree_t* avlt_copy (avltree_t *tree);

/*
 * Inserts a new node before the node at index index, or after
 * the last node if index == avlt_num_elements(tree) + 1.
 * For use only with unsorted trees.
 */
avltnode_t* avlt_insert (avltree_t *tree, void *data, unsigned int index);

/*
 * Inserts a new node at the position dictated by the
 * compare function.
 * For use only with sorted trees.
 */
avltnode_t* avlt_put (avltree_t *tree, void *data, void *cmp_args);

avltnode_t* avlt_nth (avltree_t *tree, unsigned int index);

#define avlt_first(t)     avlt_nth((t),0)
#define avlt_last(t)      avlt_nth((t),(t)->num_elements)

avltnode_t* avlt_lookup (avltree_t *tree, void *target, void *cmp_args);

/*
 * Returns the index of node in tree.
 */
unsigned int avlt_index (avltree_t *tree, avltnode_t *node);

avltnode_t* avlt_succ (avltree_t *tree, avltnode_t *node);
avltnode_t* avlt_pred (avltree_t *tree, avltnode_t *node);

void avlt_delete (avltree_t *tree, avltnode_t *node);

unsigned int avlt_nodes (avltree_t *tree);

typedef void (*avlt_walk_func_t) (void *data, void *params);
void avlt_walk (avltree_t *tree, avlt_walk_func_t walker, void *walk_args);

void* avltnode_data (avltnode_t *node);

#endif
