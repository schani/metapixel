/* -*- c -*- */

/*
 * avl.c
 *
 * Copyright (C) 1997-2004 Mark Probst
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

#include <stdlib.h>
#include <assert.h>

#include "avl.h"

#define LINK(a,N)            (((a) == -1) ? (N)->left : (N)->right)
#define LINK_SET(s,P,V)      { if ((s) == -1) (P)->left = (V); else (P)->right = (V); }

static int
avlt_compare (avltree_t *tree, void *data, unsigned int rank, avltnode_t *node, void *params)
{
    if (tree->compare != 0)
	return tree->compare(data, node->data, params);
    if (rank < node->rank)
	return -1;
    if (rank == node->rank)
	return 0;
    return 1;
}

static int
avlt_balance (avltnode_t *s, int a, avltnode_t **head)
{
    avltnode_t *r, *p, *t;

    *head = 0;
    t = s->up;
    r = LINK(a, s);		/* R = LINK(a,S) */
    if (r->balance == 0 || r->balance == a) /* single rotation */
    {
	r->up = t;		/* UP(R) = T */
	s->up = r;		/* UP(S) = R */
	if (t != 0)
	{
	    if (t->left == s)
		t->left = r;	/* LEFT(T) = R */
	    else                
		t->right = r;	/* RIGHT(T) = R */
	}
	else
	    *head = r;
	if (LINK(-a, r))
	    LINK(-a, r)->up = s; /* UP(LINK(-a,R)) = S */
	p = r;
	LINK_SET(a, s, LINK(-a, r));
	LINK_SET(-a, r, s);
	if (a == 1)
	    r->rank += s->rank;
	else
	    s->rank -= r->rank;
	if (r->balance == a)
	{
	    s->balance = r->balance = 0;
	    return a;
	}
	r->balance = -a;
	return 0;
    }

    /* double rotation */
    p = LINK(-a, r);
    p->up = t;			/* UP(P) = T */
    s->up = r->up = p;		/* UP(S) = UP(R) = P */
    if (t != 0)
    {
	if (t->left == s)
	    t->left = p;	/* LEFT(T) = R */
	else                
	    t->right = p;	/* RIGHT(T) = R */
    }
    else
	*head = p;
    if (LINK(a, p))
	LINK(a, p)->up = r;	/* UP(LINK(a,P)) = R */
    if (LINK(-a, p))
	LINK(-a, p)->up = s;	/* UP(LINK(-a,P)) = S */
    LINK_SET(-a, r, LINK(a, p));
    LINK_SET(a, p, r);
    LINK_SET(a, s, LINK(-a, p));
    LINK_SET(-a, p, s);
    if (p->balance == a)
    {
	s->balance = -a;
	r->balance = 0;
    }
    else
	if (p->balance == -a)
	{
	    s->balance = 0;
	    r->balance = a;
	}
	else
	    s->balance = r->balance = 0;
    p->balance = 0;
    if (a == 1)
    {
	r->rank -= p->rank;
	p->rank += s->rank;
    }
    else
    {
	p->rank += r->rank;
	s->rank -= p->rank;
    }
    return -a;
}

avltree_t*
avlt_create_unsorted (void)
{
    avltree_t *tree = (avltree_t*)malloc(sizeof(avltree_t));

    if (tree == 0)
	return 0;

    tree->head = 0;
    tree->num_elements = 0;
    tree->compare = 0;

    return tree;
}

avltree_t*
avlt_create_sorted (avlt_compare_func_t compare)
{
    avltree_t *tree = (avltree_t*)malloc(sizeof(avltree_t));

    if (tree == 0)
	return 0;

    tree->head = 0;
    tree->num_elements = 0;
    tree->compare = compare;

    return tree;
}

static void
avlt_destroy_node (avltnode_t *node)
{
    if (node->left != 0)
	avlt_destroy_node(node->left);
    if (node->right != 0)
	avlt_destroy_node(node->right);
    free(node);
}

void
avlt_destroy (avltree_t *tree)
{
    if (tree->head != 0)
	avlt_destroy_node(tree->head);
    free(tree);
}

static avltnode_t*
avlt_copy_node (avltnode_t *node)
{
    avltnode_t *newnode;

    if (node == 0)
	return 0;

    newnode = (avltnode_t*)malloc(sizeof(avltnode_t));

    newnode->data = node->data;
    newnode->balance = node->balance;
    newnode->rank = node->rank;

    newnode->up = 0;

    if (node->left == 0)
	newnode->left = 0;
    else
    {
	newnode->left = avlt_copy_node(node->left);
	newnode->left->up = newnode;
    }

    if (node->right == 0)
	newnode->right = 0;
    else
    {
	newnode->right = avlt_copy_node(node->right);
	newnode->right->up = newnode;
    }

    return newnode;
}

avltree_t*
avlt_copy (avltree_t *tree)
{
    avltree_t *newtree = (avltree_t*)malloc(sizeof(avltree_t));

    newtree->num_elements = tree->num_elements;
    newtree->compare = tree->compare;

    newtree->head = avlt_copy_node(tree->head);

    return newtree;
}

static avltnode_t*
avlt_insert_internal (avltree_t *tree, void* data, unsigned int pos, void *cmp_args)
{
    avltnode_t *node, *s, *p, *r;
    unsigned int u, m;
    int a, result;
    int done = 0;

    node = (avltnode_t*)malloc(sizeof(avltnode_t));

    if (node == 0)
	return 0;

    node->data = data;
    node->left = 0;
    node->right = 0;
    node->balance = 0;
    node->rank = 1;

    ++tree->num_elements;

    if (tree->head == 0)
    {
	node->up = 0;
	tree->head = node;
    }
    else
    {
	u = m = pos;
	s = p = tree->head;
	do
	{
	    if (avlt_compare(tree, data, m, p, cmp_args) < 0)
	    {			/* move left */
		p->rank++;
		r = p->left;
		if (r == 0)
		{
		    p->left = node;
		    node->up = p;
		    done = 1;
		}
		else
		    if (r->balance != 0)
		    {
			s = r;
			u = m;
		    }
		p = r;
	    }
	    else
	    {			/* move right */
		m -= p->rank;
		r = p->right;
		if (r == 0)
		{
		    p->right = node;
		    node->up = p;
		    done = 1;
		}
		else
		    if (r->balance != 0)
		    {
			s = r;
			u = m;
		    }
		p = r;
	    }
	} while (!done);
	m = u;
	if (avlt_compare(tree, data, m, s, cmp_args) < 0)
	    r = p = s->left;
	else
	{
	    r = p = s->right;
	    m -= s->rank;
	}
	while (p != node)
	{
	    result = avlt_compare(tree, data, m, p, cmp_args);
	    if (result < 0)
	    {
		p->balance = -1;
		p = p->left;
	    }
	    else
	    {
		p->balance = 1;
		m -= p->rank;
		p = p->right;
	    }
	}

	/* balancing act */
	if (avlt_compare(tree, data, u, s, cmp_args) < 0)
	    a = -1;
	else
	    a = 1;
	if (s->balance == 0)
	{
	    s->balance = a;
	    return node;
	}
	if (s->balance == -a)
	{
	    s->balance = 0;
	    return node;
	}

	/* we need to rebalance our tree */
	avlt_balance(s, a, &r);
	if (r)
	    tree->head = r;
    }

    return node;
}

avltnode_t*
avlt_insert (avltree_t *tree, void *data, unsigned int index)
{
    assert(tree->compare == 0);
    assert(index >= 0 && index <= tree->num_elements);

    ++index;

    return avlt_insert_internal(tree, data, index, 0);
}

avltnode_t*
avlt_put (avltree_t *tree, void* data, void *cmp_args)
{
    assert(tree->compare != 0);

    return avlt_insert_internal(tree, data, tree->num_elements + 1, cmp_args);
}

avltnode_t*
avlt_nth (avltree_t *tree, unsigned int index)
{
    avltnode_t *node;

    assert(index >= 0 && index < tree->num_elements);

    ++index;

    if (tree->head == 0)
	return 0;

    node = tree->head;
    while (index != node->rank)
    {
	if (index < node->rank)
	    node = node->left;
	else
	{
	    index -= node->rank;
	    node = node->right;
	}
    }

    return node;
}

avltnode_t*
avlt_lookup (avltree_t *tree, void *data, void *cmp_args)
{
    avltnode_t *node, *last = 0;
    int result;

    assert(tree->compare != 0);

    if (tree->head == 0)
	return 0;

    node = tree->head;
    result = tree->compare(data, node->data, cmp_args);
    while (node)
    {
	if (result == 0)
	    last = node;
	if (result <= 0)
	    node = node->left;
	else
	    node = node->right;
	if (node)
	    result = tree->compare(data, node->data, cmp_args);
    }

    return last;
}

unsigned int
avlt_index (avltree_t *tree, avltnode_t *node)
{
    unsigned int index = node->rank;

    while (node->up != 0)
    {
	if (node->up->right == node)
	    index += node->up->rank;
	node = node->up;
    }

    return index - 1;
}

avltnode_t*
avlt_succ (avltree_t *tree, avltnode_t *node)
{
    if (node->right != 0)
    {
	node = node->right;
	while (node->left != 0)
	    node = node->left;
	return node;
    }

    do
    {
	if (node->up != 0)
	{
	    if (node->up->left == node)
		return node->up;
	    else
		node = node->up;
	}
	else
	    return 0;
    } while (1);
}

avltnode_t*
avlt_pred (avltree_t *tree, avltnode_t *node)
{
    if (node->left != 0)
    {
	node = node->left;
	while (node->right != 0)
	    node = node->right;
	return node;
    }

    do
    {
	if (node->up != 0)
	{
	    if (node->up->right == node)
		return node->up;
	    else
		node = node->up;
	}
	else
	    return 0;
    } while (1);
}

static void
avlt_delete_internal (avltree_t *tree, avltnode_t *node, int a)
{
    int balancing = 1;
    avltnode_t *temp;

    if (node->up == 0)		/* deleting root */
    {
	tree->head = LINK(a, node);
	if (tree->head != 0)
	    tree->head->up = 0;
	free(node);
	return;
    }

    temp = node;
    if (node->up->left == node)
    {
	node->up->left = LINK(a, node);
	a = -1;
    }
    else
    {
	node->up->right = LINK(a, node);
	a = 1;
    }
    node = node->up;
    if (LINK(a, node))
	LINK(a, node)->up = node;
    free(temp);
    do
    {
	if (a == -1)
	    node->rank--;	/* adjust rank */
	if (balancing)
	{			/* adjust balance */
	    if (node->balance == 0)
	    {
		node->balance = -a;
		balancing = 0;
	    }
	    else
		if (node->balance == a)
		    node->balance = 0;
		else
		{		/* rebalancing is necessary */
		    if (!avlt_balance(node, -a, &temp))
			balancing = 0;
		    if (temp != 0)
			tree->head = temp;
		    node = node->up;
		}
	}
	if (node->up != 0)
	{			/* go up one level */
	    if (node->up->left == node)
		a = -1;
	    else
		a = 1;
	    node = node->up;
	}
	else
	    node = 0;
    } while (node);
}

static void
avlt_exchange_nodes (avltree_t *tree, avltnode_t *node1, avltnode_t *node2)
{
    avltnode_t tmp;
    int tricky = 0;

    if (node1->up == node2)
    {
	avltnode_t *tmpp = node1;

	node1 = node2;
	node2 = tmpp;
    }

    if (node2->up == node1)
	tricky = 1;

    tmp = *node1;

    node1->balance = node2->balance;
    node1->rank = node2->rank;

    if (node1->left != 0)
	node1->left->up = node2;
    if (node1->right != 0)
	node1->right->up = node2;
    if (node1->up != 0)
    {
	if (node1->up->left == node1)
	    node1->up->left = node2;
	else
	    node1->up->right = node2;
    }
    else
	tree->head = node2;

    node1->left = node2->left;
    node1->right = node2->right;
    if (!tricky)
	node1->up = node2->up;
    else
	node1->up = node2;

    node2->balance = tmp.balance;
    node2->rank = tmp.rank;

    if (node2->left != 0)
	node2->left->up = node1;
    if (node2->right != 0)
	node2->right->up = node1;
    if (node2->up != 0)
    {
	if (!tricky)
	{
	    if (node2->up->left == node2)
		node2->up->left = node1;
	    else
		node2->up->right = node1;
	}
    }
    else
	tree->head = node1;

    node2->left = tmp.left;
    node2->right = tmp.right;
    if (tricky)
    {
	if (tmp.left == node2)
	    node2->left = node1;
	else
	    node2->right = node1;
    }

    node2->up = tmp.up;
}

static void
avlt_delete_at_index (avltree_t *tree, unsigned int index)
{
    avltnode_t *node, *next;

    assert(index >= 1 && index <= tree->num_elements);

    node = avlt_nth(tree, index - 1);
    tree->num_elements--;
    next = node->right;
    if (next != 0)
    {
	for (; next->left; next = next->left)
	    ;

	avlt_exchange_nodes(tree, node, next);
	avlt_delete_internal(tree, node, 1);
    }
    else
	avlt_delete_internal(tree, node, -1);
}

void
avlt_delete (avltree_t *tree, avltnode_t *node)
{
    avlt_delete_at_index(tree, avlt_index(tree, node) + 1); /* should not need to get index */
}

unsigned int
avlt_nodes (avltree_t *tree)
{
    return tree->num_elements;
}

static void
avlt_walk_internal (avltnode_t *node, avlt_walk_func_t walker, void *walk_args)
{
    if (node->left != 0)
	avlt_walk_internal(node->left, walker, walk_args);
    walker(node->data, walk_args);
    if (node->right != 0)
	avlt_walk_internal(node->right, walker, walk_args);
}

void
avlt_walk (avltree_t *tree, avlt_walk_func_t walker, void *walk_args)
{
    assert(walker != 0);

    if (tree->head != 0)
	avlt_walk_internal(tree->head, walker, walk_args);
}

void*
avltnode_data (avltnode_t *node)
{
    return node->data;
}

static int
avlt_is_balanced (avltnode_t *node, unsigned int *height, unsigned int *num_elements)
{
    unsigned int height_l, height_r, num_elements_r;

    if (node == 0)
    {
	*height = 0;
	*num_elements = 0;
	return 1;
    }

    if (node->left != 0)
    {
	if (node->left->up != node)
	    return 0;
	if (!avlt_is_balanced(node->left, &height_l, num_elements))
	    return 0;
	if (node->rank != *num_elements + 1)
	    return 0;
    }
    else
    {
	if (node->rank != 1)
	    return 0;
	height_l = 0;
	*num_elements = 0;
    }
    if (node->right != 0)
    {
	if (node->right->up != node)
	    return 0;
	if (!avlt_is_balanced(node->right, &height_r, &num_elements_r))
	    return 0;
    }
    else
    {
	num_elements_r = 0;
	height_r = 0;
    }
    if ((int)height_r - (int)height_l != node->balance)
	return 0;
    if (height_r > height_l)
	*height = height_r + 1;
    else
	*height = height_l + 1;
    *num_elements += num_elements_r + 1;
    return 1;
}

#ifdef AVL_TEST

#include <ctype.h>
#include <string.h>
#include <stdio.h>

int
compare_strings (void *string1, void *string2, void *params)
{
    return strcmp((const char*)string1, (const char*)string2);
}

void
printf_walker (char *data, void *params)
{
    printf("%s", data);
}

int
main (void)
{
    avltree_t *tree;
    avltnode_t *node;
    unsigned int height, counter, index, num_elements;
    FILE *file;
    char buffer[128], *string;

    tree = avlt_create_sorted(compare_strings);
    file = fopen("avl.c", "r");
    counter = 0;
    do
    {
	if (fgets(buffer, 128, file))
	{
	    for (index = 0; isspace(buffer[index]); index++)
		;
	    string = (char*)malloc(strlen(buffer + index) + 1);
	    strcpy(string, buffer + index);
	    avlt_put(tree, string, 0);
	    ++counter;
	    assert(avlt_is_balanced(tree->head, &height, &num_elements));
	}
	else
	    break;
    } while (1);
    fclose(file);

    while (avlt_nodes(tree) > 0)
    {
	avlt_delete(tree, avlt_nth(tree, random() % avlt_nodes(tree)));
	assert(avlt_is_balanced(tree->head, &height, &num_elements));
    }

    /*
    avlt_walk(tree, (Avlt_walk_f)printf_walker, 0);
    */

    avlt_destroy(tree);

    tree = avlt_create_unsorted();
    file = fopen("avl.c", "r");
    do
    {
	if (fgets(buffer, 128, file))
	{
	    string = (char*)malloc(strlen(buffer) + 1);
	    strcpy(string, buffer);
	    avlt_insert(tree, string, avlt_nodes(tree));
	    assert(avlt_is_balanced(tree->head, &height, &num_elements));
	}
	else
	    break;
    } while (1);
    fclose(file);

    /*
    avlt_walk(tree, (Avlt_walk_f)printf_walker, 0);
    */

    avlt_destroy(tree);

    return 0;
}

#endif
