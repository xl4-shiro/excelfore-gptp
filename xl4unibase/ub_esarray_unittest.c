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

static void test_esarray1(void **state)
{
	ub_esarray_cstd_t *eah;
	int *pi;
	int i;
	int max_pnum = 11;

	eah = ub_esarray_init(4,4,max_pnum);

	for (i=0;i<max_pnum;i++){
		ub_esarray_add_ele(eah, (ub_esarray_element_t *)&i);
	}

	// a new element exceed max elements
	assert_null(ub_esarray_get_newele(eah));

	assert_int_equal(ub_esarray_ele_nums(eah), max_pnum);

	for (i=0;i<max_pnum;i++){
		pi=(int*)ub_esarray_get_ele(eah, i);
		assert_int_equal(i, *pi);
	}

	pi = (int*)ub_esarray_get_ele(eah, 2);
	ub_esarray_del_ele(eah, (ub_esarray_element_t *)pi); // 3rd point is removed
	for (i=0;i<2;i++){
		// before 3rd pointer
		pi = (int*)ub_esarray_get_ele(eah, i);
		assert_int_equal(i, *pi);
	}
	for (i=2;i<max_pnum-1;i++){
		// 3rd to (MAX_PNUM-1)th
		pi = (int*)ub_esarray_get_ele(eah, i);
		assert_int_equal(i+1, *pi);
	}

	assert_int_equal(max_pnum-1, ub_esarray_ele_nums(eah));

	for (i=0;i<max_pnum-1;i++){
		pi = (int*)ub_esarray_get_ele(eah, 0);
		if(i<2){
			assert_int_equal(i, *pi);
		}
		if(i>=2){
			assert_int_equal(i+1, *pi);
		}
		ub_esarray_del_ele(eah, (ub_esarray_element_t *)pi);
	}

	assert_int_equal(0, ub_esarray_ele_nums(eah));

	ub_esarray_close(eah);
}

struct test_data {
	char x[7];
	int a;
	char b;
	uint16_t c;
};
static void test_esarray2(void **state)
{
	int i;
	ub_esarray_cstd_t *eah;
	struct test_data d0={.x={'a','b','c',0}, .a=10, .b=20, .c=30};
	struct test_data d1={.x={'a','b','c',0}, .a=10, .b=20, .c=30};
	eah = ub_esarray_init(8,sizeof(struct test_data),32);
	for(i=0;i<17;i++){
		d1.a+=1;
		ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d1);
	}
	ub_esarray_data_lock(eah);
	assert_int_equal(-1, ub_esarray_pop_ele(eah, (ub_esarray_element_t *)&d1));
	ub_esarray_data_unlock(eah);
	assert_int_equal(0, ub_esarray_pop_ele(eah, (ub_esarray_element_t *)&d1));
	assert_string_equal(d0.x, d1.x);
	assert_int_equal(d0.a+1, d1.a);
	assert_int_equal(d0.b, d1.b);
	assert_int_equal(d0.c, d1.c);
	assert_int_equal(0, ub_esarray_pop_last_ele(eah, (ub_esarray_element_t *)&d1));
	assert_string_equal(d0.x, d1.x);
	assert_int_equal(d0.a+17, d1.a);
	assert_int_equal(d0.b, d1.b);
	assert_int_equal(d0.c, d1.c);

	// remove all
	ub_esarray_close(eah);
	eah = ub_esarray_init(8,sizeof(struct test_data),32);
	ub_esarray_data_lock(eah);
	for(i=0;i<8;i++){
		// the first 8 should be okay
		assert_int_equal(0, ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d0));
	}
	// data is locked, and can't reallocate the area
	assert_int_equal(-1, ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d0));

	// can't pop in the locked status
	assert_int_equal(-1, ub_esarray_pop_ele(eah, (ub_esarray_element_t *)&d1));

	// pop one, then one can be added
	ub_esarray_data_unlock(eah);
	assert_int_equal(0, ub_esarray_pop_ele(eah, (ub_esarray_element_t *)&d1));
	ub_esarray_data_lock(eah);
	assert_int_equal(0, ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d0));

	// the next one again fails
	assert_int_equal(-1, ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d0));
	ub_esarray_data_unlock(eah);
	// after unlock, it should be okay
	assert_int_equal(0, ub_esarray_add_ele(eah, (ub_esarray_element_t *)&d0));

	ub_esarray_close(eah);
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_esarray1),
		cmocka_unit_test(test_esarray2),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
