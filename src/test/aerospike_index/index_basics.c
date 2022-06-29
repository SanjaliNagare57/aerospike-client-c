/*
 * Copyright 2008-2022 Aerospike, Inc.
 *
 * Portions may be licensed to Aerospike, Inc. under one or more contributor
 * license agreements.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
#include <aerospike/aerospike.h>
#include <aerospike/aerospike_index.h>
#include <aerospike/aerospike_info.h>
#include <aerospike/aerospike_key.h>

#include <aerospike/as_error.h>
#include <aerospike/as_status.h>

#include <aerospike/as_record.h>
#include <aerospike/as_integer.h>
#include <aerospike/as_string.h>
#include <aerospike/as_list.h>
#include <aerospike/as_arraylist.h>
#include <aerospike/as_map.h>
#include <aerospike/as_hashmap.h>
#include <aerospike/as_val.h>

#include "../test.h"
#include "../util/index_util.h"

/******************************************************************************
 * GLOBAL VARS
 *****************************************************************************/

extern aerospike* as;

/******************************************************************************
 * MACROS
 *****************************************************************************/
#define NAMESPACE "test"
#define SET "test_index"

/******************************************************************************
 * TYPES
 *****************************************************************************/


/******************************************************************************
 * STATIC FUNCTIONS
 *****************************************************************************/


/******************************************************************************
 * TEST CASES
 *****************************************************************************/

TEST(index_basics_create, "Create index on bin")
{
	as_error err;
	as_error_reset(&err);

	as_index_task task;

	// DEFAULT type index
	as_status status = aerospike_index_create(as, &err, &task, NULL, NAMESPACE, SET, "new_bin", "idx_test_new_bin", AS_INDEX_STRING);

	if (! index_process_return_code(status, &err, &task)) {
		assert_int_eq(status , AEROSPIKE_OK);
	}

	// LIST type index
	status = aerospike_index_create_complex(as, &err, &task, NULL, NAMESPACE,
			SET, "new_bin[0]", "idx_test_listbin", AS_INDEX_TYPE_LIST,
			AS_INDEX_STRING);

	if (! index_process_return_code(status, &err, &task)) {
		assert_int_eq(status , AEROSPIKE_OK);
	}
}

TEST(index_basics_drop , "Drop index")
{
	as_error err;
	as_error_reset(&err);

	// DEFAULT type index
	aerospike_index_remove(as, &err, NULL, NAMESPACE, "idx_test_new_bin");
	if ( err.code != AEROSPIKE_OK ) {
		info("error(%d): %s", err.code, err.message);
	}
	assert_int_eq( err.code, AEROSPIKE_OK );

	// LIST type index
	aerospike_index_remove(as, &err, NULL, NAMESPACE, "idx_test_listbin");
	if ( err.code != AEROSPIKE_OK ) {
		info("error(%d): %s", err.code, err.message);
	}
	assert_int_eq( err.code, AEROSPIKE_OK );
}

TEST(index_ctx_test , "Create ctx index on bin")
{
	as_error err;
	as_error_reset(&err);

	char* res = NULL;

	aerospike_info_any(as, &err, NULL, "sindex/test/idx_test_ctx", &res);

	if (res != NULL) {
		assert_not_null(res);
		info("sindex-info: %s", res);
		free(res);
		res = NULL;
	}

	as_index_task task;

	as_cdt_ctx ctx;
	as_cdt_ctx_init(&ctx, 1);
	as_cdt_ctx_add_list_index(&ctx, 0);

	aerospike_index_remove(as, &err, NULL, NAMESPACE, "idx_test_ctx");

	if (err.code != AEROSPIKE_OK) {
		info("error(%d): %s", err.code, err.message);
	}

	assert_int_eq( err.code, AEROSPIKE_OK );

	as_status status = aerospike_index_create_ctx(as, &err, &task, NULL,
			NAMESPACE, SET, "new_bin", "idx_test_ctx", AS_INDEX_TYPE_DEFAULT,
			AS_INDEX_NUMERIC, &ctx);

	if (! index_process_return_code(status, &err, &task)) {
		assert_int_eq(status , AEROSPIKE_OK);
	}

	as_arraylist list;
	as_key rkey;
	as_record rec;

	for (uint32_t i = 0; i < 100; i++) {
		as_key_init_int64(&rkey, NAMESPACE, SET, i + 2000);

		as_arraylist_init(&list, 1, 1);
		as_arraylist_append_int64(&list, i);

		as_record_init(&rec, 1);
		as_record_set_list(&rec, "new_bin", (as_list*)&list);

		status = aerospike_key_put(as, &err, NULL, &rkey, &rec);
		assert_int_eq(status, AEROSPIKE_OK);
		as_record_destroy(&rec);
	}

	as_cdt_ctx_destroy(&ctx);

	aerospike_info_any(as, &err, NULL, "sindex/test/idx_test_ctx", &res);
	assert_not_null(res);
	info("sindex-info: %s", res);
	free(res);
	res = NULL;
}


/******************************************************************************
 * TEST SUITE
 *****************************************************************************/

SUITE(index_basics, "aerospike_sindex basic tests")
{
	suite_add(index_basics_create);
	suite_add(index_basics_drop);
	suite_add(index_ctx_test);
}
