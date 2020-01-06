/*
 * excelfore-gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of excelfore-gptp.
 *
 * excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
/*
 * ub_llist.c
 * memory buffer utilities
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Norman Salunga (norman.salunga@excelfore.com)
 */
#include <stdio.h>
#include "ub_llist.h"

void ub_list_init(struct ub_list *ub_list)
{
	if(!ub_list) return;
	ub_list->head = NULL;
	ub_list->tail = NULL;
}

void ub_list_clear(struct ub_list *ub_list, ub_list_node_clear_h node_clear,
		   void *arg)
{
	struct ub_list_node *node;
	if(!ub_list) return;
	node = ub_list->head;
	while(node){
		struct ub_list_node *next = node->next;
		if(node_clear) node_clear(node, arg);
		node = next;
	}
	ub_list_init(ub_list);
}

void ub_list_append(struct ub_list *ub_list, struct ub_list_node *node)
{
	if(!ub_list || !node) return;
	node->prev = ub_list->tail;
	node->next = NULL;

	if(!ub_list->head)
		ub_list->head = node;
	if(ub_list->tail)
		ub_list->tail->next = node;
	ub_list->tail = node;
}

void ub_list_prepend(struct ub_list *ub_list, struct ub_list_node *node)
{
	if(!ub_list || !node) return;
	node->prev = NULL;
	node->next = ub_list->head;

	if(ub_list->head)
		ub_list->head->prev = node;
	if(!ub_list->tail)
		ub_list->tail = node;
	ub_list->head = node;
}

void ub_list_insert_before(struct ub_list *ub_list, struct ub_list_node *refnode,
			   struct ub_list_node *node)
{
	if(!ub_list || !refnode || !node) return;
	if(refnode->prev)
		refnode->prev->next = node;
	else if(ub_list->head == refnode)
		ub_list->head = node;

	node->prev = refnode->prev;
	node->next = refnode;
	refnode->prev = node;
}

void ub_list_insert_after(struct ub_list *ub_list, struct ub_list_node *refnode,
			  struct ub_list_node *node)
{
	if(!ub_list || !refnode || !node) return;
	if(refnode->next)
		refnode->next->prev = node;
	else if(ub_list->tail == refnode)
		ub_list->tail = node;

	node->prev = refnode;
	node->next = refnode->next;
	refnode->next = node;
}

void ub_list_unlink(struct ub_list *ub_list, struct ub_list_node *node)
{
	if(!node) return;
	if(node->prev)
		node->prev->next = node->next;
	else
		ub_list->head = node->next;

	if(node->next)
		node->next->prev = node->prev;
	else
		ub_list->tail = node->prev;

	node->next = NULL;
	node->prev = NULL;
}

void ub_list_sort(struct ub_list *ub_list, ub_list_sort_h *sh, void *arg)
{
	struct ub_list_node *node;
	bool sort;

	if(!ub_list || !sh) return;

retry:
	node = ub_list->head;
	sort = false;
	while(node && node->next){
		if(sh(node, node->next, arg)){
			node = node->next;
		}else{
			struct ub_list_node *tnode = node->next;
			ub_list_unlink(ub_list, node);
			ub_list_insert_after(ub_list, tnode, node);
			sort = true;
		}
	}

	if(sort){
		goto retry;
	}
}

struct ub_list_node *ub_list_apply(const struct ub_list *ub_list, bool fwd,
				   ub_list_apply_h *ah, void *arg)
{
	struct ub_list_node *node;

	if(!ub_list || !ah) return NULL;

	node = fwd ? ub_list->head : ub_list->tail;
	while(node){
		struct ub_list_node *cur = node;
		node = fwd ? node->next : node->prev;
		if(ah(cur, arg)) return cur;
	}
	return NULL;
}

struct ub_list_node *ub_list_head(const struct ub_list *ub_list)
{
	return ub_list ? ub_list->head : NULL;
}

struct ub_list_node *ub_list_tail(const struct ub_list *ub_list)
{
	return ub_list ? ub_list->tail : NULL;
}

uint32_t ub_list_count(const struct ub_list *ub_list)
{
	uint32_t n=0;
	struct ub_list_node *node;

	if(!ub_list) return 0;
	for(node = ub_list->head; node; node = node->next)
		n++;

	return n;
}
