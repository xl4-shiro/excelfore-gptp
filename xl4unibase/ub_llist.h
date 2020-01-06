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
/**
 * @defgroup llist Doubly Linked List
 * @{
 * @file ub_llist.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Norman Salunga (norman.salunga@excelfore.com)
 *
 * @brief doubly linked list
 */

#ifndef __LUB_LIST_UTILS_H_
#define __LUB_LIST_UTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * @brief ub_list node structure
 */
struct ub_list_node {
	struct ub_list_node *prev;     /**< Pointer to previous node */
	struct ub_list_node *next;     /**< Pointer to next node */
	void *data;		       /**< Pointer to data */
};

/**
 * @brief Initialize members of ub_list node
 */
#define UB_LIST_NODE_INIT {NULL, NULL, NULL}

/**
 * @brief Link ub_list structure
 */
struct ub_list {
	struct ub_list_node *head;    /**< Pointer to first node */
	struct ub_list_node *tail;    /**< Pointer to last node */
};

/**
 * @brief Initialize ub_list
 */
#define UB_LIST_INIT {NULL, NULL}

/**
 * @brief Initialize ub_list
 * @param ub_list pointer to link ub_list structure
 */
void ub_list_init(struct ub_list *ub_list);

/**
 * @brief function to clean up node
 * @param node node to be applied
 * @param arg clean up argument
 */
typedef void (ub_list_node_clear_h)(struct ub_list_node *node, void *arg);

/**
 * @brief Clear the ub_list without freeing any nodes
 * @param ub_list pointer to link ub_list structure
 * @param node_clear clean up callback fuction
 * @param arg clean up argument
 */
void ub_list_clear(struct ub_list *ub_list, ub_list_node_clear_h node_clear,
		   void *arg);

/**
 * @brief Append a node at the end of the ub_list
 * @param ub_list Pointer to link ub_list structure
 * @param node entry to be appended
 */
void ub_list_append(struct ub_list *ub_list, struct ub_list_node *node);

/**
 * @brief Append a node at the start of the ub_list
 * @param ub_list Pointer to link ub_list structure
 * @param node entry to be appended
 */
void ub_list_prepend(struct ub_list *ub_list, struct ub_list_node *node);

/**
 * @brief Append a node before a reference node
 * @param ub_list Pointer to link ub_list structure
 * @param refnode Reference node
 * @param node entry to be inserted
 */
void ub_list_insert_before(struct ub_list *ub_list, struct ub_list_node *refnode,
			   struct ub_list_node *node);

/**
 * @brief Append a node after a reference node
 * @param ub_list Pointer to link ub_list structure
 * @param refnode Reference node
 * @param node entry to be appended
 */
void ub_list_insert_after(struct ub_list *ub_list, struct ub_list_node *refnode,
			  struct ub_list_node *node);

/**
 * @brief Remove a node from the ub_list
 * @param ub_list Pointer to link ub_list structure
 * @param node entry to be removed
 */
void ub_list_unlink(struct ub_list *ub_list, struct ub_list_node *node);

/**
 * @brief Comparator function handler for nodes
 * @param node1 first node
 * @param node2 second node
 * @param arg Comparison argument
 * @note the result must end up with all true, otherwise it goes to infinite loop
 */
typedef bool (ub_list_sort_h)(struct ub_list_node *node1,
			      struct ub_list_node *node2, void *arg);

/**
 * @brief Sort entries in the ub_list based on comparator
 * @param ub_list Pointer to link ub_list structure
 * @param sh Sort comparator handler
 * @param arg Sort argument
 */
void ub_list_sort(struct ub_list *ub_list, ub_list_sort_h *sh, void *arg);

/**
 * @brief Apply function handler for nodes
 * @param node node to be applied
 * @param arg Apply argument
 */
typedef bool (ub_list_apply_h)(struct ub_list_node *node, void *arg);

/**
 * @brief Apply function to nodes in the ub_list
 * @param ub_list Pointer to link ub_list structure
 * @param fwd Forward or reverse movement
 * @param ah Apply function handler
 * @param arg Apply argument
 */
struct ub_list_node *ub_list_apply(const struct ub_list *ub_list, bool fwd,
				   ub_list_apply_h *ah, void *arg);

/**
 * @brief Get first entry of the ub_list
 * @param ub_list Pointer to link ub_list structure
 */
struct ub_list_node *ub_list_head(const struct ub_list *ub_list);

/**
 * @brief Get last entry of the ub_list
 * @param ub_list Pointer to link ub_list structure
 */
struct ub_list_node *ub_list_tail(const struct ub_list *ub_list);

/**
 * @brief Get number of entries
 * @param ub_list Pointer to link ub_list structure
 * @return number of entries
 */
uint32_t ub_list_count(const struct ub_list *ub_list);

/**
 * @brief Get data of node
 * @param node Node entry
 * @return pointer to data
 */
static inline void *ub_list_nodedata(const struct ub_list_node *node){
	return node ? node->data : NULL;
}

/**
 * @brief Check if ub_list is empty
 * @param ub_list Pointer to link ub_list structure
 * @return true if node exist, otherwise false
 */
static inline bool ub_list_isempty(const struct ub_list *ub_list){
	return ub_list ? ub_list->head == NULL : true;
}

/**
 * @brief Traverse the ub_list
 */
#define UB_LIST_FOREACH(ub_list, node) \
	for((node) = ub_list_head((ub_list)); (node); (node) = (node)->next)

#endif
/** @}*/
