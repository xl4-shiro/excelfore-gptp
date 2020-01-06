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
#include "unibase.h"
#include "ub_unittest_helper.c"

static bool test_comp(struct ub_list_node *node1, struct ub_list_node *node2,
		       void *arg)
{
	return *((int*)node1->data) <= *((int*)node2->data);
}

static bool test_revcomp(struct ub_list_node *node1, struct ub_list_node *node2,
		       void *arg)
{
	return *((int*)node1->data) >= *((int*)node2->data);
}

static bool test_apply(struct ub_list_node *node, void *arg)
{
	int i=*(int*)arg;
	if(i==2 && *((int*)node->data)==50) return -1;
	if(i==3 && *((int*)node->data)==102) return -1;
	*((int*)node->data)*=i;
	return 0;
}

static void test_node_clear(struct ub_list_node *node, void *arg)
{
	*(int*)arg+=1;
}

static void test_llist1(void **state)
{
	const int nnum=100;
	const int hnnum=nnum/2;
	struct ub_list_node node[nnum];
	struct ub_list_node *nodep;
	int ndata[nnum];
	struct ub_list list;
	int i,j,a;
	int dp1,dp2;

	ub_list_init(&list);
	for(i=0;i<nnum;i++) {
		ndata[i]=i;
		node[i].data=&ndata[i];
	}
	for(i=0;i<hnnum;i++){
		ub_list_append(&list, &node[i]);
	}
	for(i=0;i<hnnum;i++){
		assert_ptr_equal(node[i].data, &ndata[i]);
	}
	// delete the bottom
	ub_list_unlink(&list, &node[hnnum-1]);
	assert_ptr_equal(node[hnnum-2].next, NULL);
	// delete the top
	ub_list_unlink(&list, &node[0]);
	assert_ptr_equal(node[1].prev, NULL);
	// delete the middle
	ub_list_unlink(&list, &node[10]);
	assert_ptr_equal(node[9].next, &node[11]);
	assert_ptr_equal(node[11].prev, &node[9]);
	// now the list is 1...9,11...hnnum-2 --- hnnum-3 elements
	// add node[50] at the bottom
	ub_list_append(&list, &node[50]);
	// add node[51] at the top
	ub_list_prepend(&list, &node[51]);
	// check head and tail
	assert_ptr_equal(list.head, &node[51]);
	assert_ptr_equal(list.tail, &node[50]);
	// now the list is 51,1...9,11...hnnum-2,50 --- hnnum-1 elements
	// insert 52 before 9
	ub_list_insert_before(&list, &node[9], &node[52]);
	assert_ptr_equal(node[8].next, &node[52]);
	assert_ptr_equal(node[9].prev, &node[52]);
	// insert 53 after 11
	ub_list_insert_after(&list, &node[11], &node[53]);
	assert_ptr_equal(node[11].next, &node[53]);
	assert_ptr_equal(node[12].prev, &node[53]);
	// now the list is 51,1...8,52,9,11,53,12...hnnum-2,50 --- hnnum+1 elements
	a=2;
	ub_list_apply(&list, true, test_apply, &a);
	// now the list is 102,2,4...16,104,18,22,106,24...(hnnum-2)*2,50
	a=3;
	ub_list_apply(&list, false, test_apply, &a);
	// now the list is 102,6,12...48,312,54,66,318,72...(hnnum-2)*2*3,150
	assert_int_equal(*((int*)list.head->data), 102);
	assert_int_equal(*((int*)list.tail->data), 150);

	// sort it
	ub_list_sort(&list, test_comp, NULL);
	// now the list is 6,12...,54,66,...,96,102,102,108,...,150,150,156,...,(hnnum-2)*2*3,312,318
	// hnnum-1 + 2 elements
	dp1=0;
	dp2=0;
	nodep=list.head;
	for(i=0,j=1;i<hnnum-1;i++,j++){
		if(j*6==60) j++;
		assert_int_equal(*((int *)nodep->data), j*6);
		if(j*6==102 && dp1++==0) j--;
		if(j*6==150 && dp2++==0) j--;
		nodep=nodep->next;
	}
	assert_int_equal(*((int *)nodep->data), 312);
	nodep=nodep->next;
	assert_int_equal(*((int *)nodep->data), 318);
	assert_ptr_equal(nodep=nodep->next, NULL);

	// rev. sort it
	ub_list_sort(&list, test_revcomp, NULL);
	dp1=0;
	dp2=0;
	nodep=list.head;
	assert_int_equal(*((int *)nodep->data), 318);
	nodep=nodep->next;
	assert_int_equal(*((int *)nodep->data), 312);
	nodep=nodep->next;
	for(i=hnnum-2,j=hnnum-2;i>=0;i--,j--){
		if(j*6==60) j--;
		assert_int_equal(*((int *)nodep->data), j*6);
		if(j*6==102 && dp1++==0) j++;
		if(j*6==150 && dp2++==0) j++;
		nodep=nodep->next;
	}

	assert_int_equal(ub_list_count(&list), hnnum+1);
	a=0;
	ub_list_clear(&list, test_node_clear, &a);
	assert_int_equal(a, hnnum+1);
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_llist1),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
