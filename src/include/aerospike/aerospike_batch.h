/*
 * Copyright 2008-2021 Aerospike, Inc.
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
#pragma once

/**
 * @defgroup batch_operations Batch Operations
 * @ingroup client_operations
 *
 * The Batch API is a collection of APIs that use multiple keys for looking up
 * records in one call.
 */

#include <aerospike/aerospike.h>
#include <aerospike/as_batch.h>
#include <aerospike/as_listener.h>
#include <aerospike/as_error.h>
#include <aerospike/as_key.h>
#include <aerospike/as_list.h>
#include <aerospike/as_operations.h>
#include <aerospike/as_policy.h>
#include <aerospike/as_record.h>
#include <aerospike/as_status.h>
#include <aerospike/as_val.h>
#include <aerospike/as_vector.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * TYPES
 *****************************************************************************/

#define AS_BATCH_READ 0
#define AS_BATCH_WRITE 1
#define AS_BATCH_APPLY 2
#define AS_BATCH_REMOVE 3

/**
 * Batch record type. Values: AS_BATCH_READ, AS_BATCH_WRITE, AS_BATCH_APPLY or AS_BATCH_REMOVE
 * @ingroup batch_operations
 */
typedef uint8_t as_batch_type;

/**
 * Batch base request/response record. Used in batch commands where different command types are
 * needed for different keys. All batch record types contain these base fields.
 * @ingroup batch_operations
 */
typedef struct as_batch_base_record_s {
	/**
	 * Requested key.
	 */
	as_key key;
	
	/**
	 * Record for the requested key. This record will only be populated when the result is
	 * AEROSPIKE_OK or AEROSPIKE_ERR_UDF. If AEROSPIKE_ERR_UDF, use as_record_get_udf_error()
	 * to obtain the error message.
	 */
	as_record record;

	/**
	 * Result code.
	 */
	as_status result;

	/**
	 * Type of batch record.
	 */
	as_batch_type type;

	/**
	 * Does batch sub-transaction contain a write operation.
	 */
	bool has_write;

	/**
	 * Is it possible that the write transaction completed even though this error was generated.
	 * This may be the case when a client error occurs (like timeout) after the command was sent
	 * to the server.
	 */
	bool in_doubt;
} as_batch_base_record;

/**
 * Batch key and read only operations with read policy. 
 * @relates as_batch_base_record
 * @ingroup batch_operations
 */
typedef struct as_batch_read_record_s {
	as_key key;
	as_record record;
	as_status result;
	as_batch_type type;
	bool has_write;
	bool in_doubt; // Will always be false for reads.

	/**
	 * Optional read policy.
	 */
	as_policy_batch_read* policy;

	/**
	 * Read operations for this key. ops are mutually exclusive with bin_names.
	 */
	as_operations* ops;

	/**
	 * Bin names requested for this key. bin_names are mutually exclusive with ops.
	 */
	char** bin_names;
	
	/**
	 * Count of bin names requested for this key.
	 */
	uint32_t n_bin_names;
	
	/**
	 * If true, ignore bin_names and read all bins.
	 * If false and bin_names are set, read specified bin_names.
	 * If false and bin_names are not set, read record header (generation, expiration) only.
	 */
	bool read_all_bins;
} as_batch_read_record;

/**
 * Batch key and read/write operations with write policy.
 * @relates as_batch_base_record
 * @ingroup batch_operations
 */
typedef struct as_batch_write_record_s {
	as_key key;
	as_record record;
	as_status result;
	as_batch_type type;
	bool has_write;
	bool in_doubt;

	/**
	 * Optional write policy.
	 */
	as_policy_batch_write* policy;

	/**
	 * Required read/write operations for this key.
	 */
	as_operations* ops;
} as_batch_write_record;

/**
 * Batch UDF (user defined function) apply.
 * @relates as_batch_base_record
 * @ingroup batch_operations
 */
typedef struct as_batch_apply_record_s {
	as_key key;
	as_record record;
	as_status result;
	as_batch_type type;
	bool has_write;
	bool in_doubt;

	/**
	 * Optional apply policy.
	 */
	as_policy_batch_apply* policy;

	/**
	 * Package or lua module name.
	 */
	const char* module;

	/**
	 * Lua function name.
	 */
	const char* function;

	/**
	 * Optional arguments to lua function.
	 */
	as_list* arglist;
} as_batch_apply_record;

/**
 * Batch delete operation.
 * @relates as_batch_base_record
 * @ingroup batch_operations
 */
typedef struct as_batch_remove_record_s {
	as_key key;
	as_record record;
	as_status result;
	as_batch_type type;
	bool has_write;
	bool in_doubt;

	/**
	 * Optional remove policy.
	 */
	as_policy_batch_remove* policy;
} as_batch_remove_record;

/**
 * Batch request/response record union.
 * @ingroup batch_operations
 */
typedef union {
	as_batch_base_record base;
	as_batch_read_record read;
	as_batch_write_record write;
	as_batch_apply_record apply;
	as_batch_remove_record remove;
} as_batch_record;

/**
 * List of batch request/response (as_batch_base_record) records. The record types can be
 * as_batch_read_record, as_batch_write_record, as_batch_apply_record or as_batch_remove_record.
 */
typedef struct as_batch_records_s {
	as_vector list;
} as_batch_records;

/**
 * List of batch request/response (as_batch_base_record) records. The record types can be
 * as_batch_read_record, as_batch_write_record, as_batch_apply_record or as_batch_remove_record.
 * @deprecated Use as_batch_records instead.
 */
typedef as_batch_records as_batch_read_records;

/**
 * This callback will be called with the results of batch commands for all keys.
 *
 * 	The `results` argument will be an array of `n` as_batch_result entries. The
 * 	`results` argument is on the stack and is only available within the context
 * 	of the callback. To use the data outside of the callback, copy the data.
 *
 * ~~~~~~~~~~{.c}
 * bool my_callback(const as_batch_result* results, uint32_t n, void* udata) {
 *     return true;
 * }
 * ~~~~~~~~~~
 *
 * @param results		The results from the batch request.
 * @param n				The number of results from the batch request.
 * @param udata 		User-data provided to the calling function.
 * 
 * @return `true` on success. Otherwise, an error occurred.
 *
 * @ingroup batch_operations
 */
typedef bool (*aerospike_batch_callback)(const as_batch_result* results, uint32_t n, void* udata);

/**
 * This callback will be called with the results of aerospike_batch_get(),
 * or aerospike_batch_exists() functions.
 *
 * @deprecated Use aerospike_batch_callback instead.
 * @ingroup batch_operations
 */
typedef bool (*aerospike_batch_read_callback)(const as_batch_read* results, uint32_t n, void* udata);

/**
 * @private
 * This callback is used by aerospike_batch_get_xdr() to send one batch record at a time
 * as soon as they are received in no particular order.
 */
typedef bool (*as_batch_callback_xdr)(as_key* key, as_record* record, void* udata);
	
/**
 * Asynchronous batch user callback.  This function is called once when the batch completes or an
 * error has occurred.
 *
 * @param err			This error structure is only populated when the command fails. Null on success.
 * @param records 		Returned records.  Records must be destroyed with as_batch_read_destroy() when done.
 * @param udata 		User data that is forwarded from asynchronous command function.
 * @param event_loop 	Event loop that this command was executed on.  Use this event loop when running
 * 						nested asynchronous commands when single threaded behavior is desired for the
 * 						group of commands.
 *
 * @ingroup batch_operations
 */
typedef void (*as_async_batch_listener)(as_error* err, as_batch_read_records* records, void* udata, as_event_loop* event_loop);

/******************************************************************************
 * FUNCTIONS
 *****************************************************************************/

/**
 * Initialize batch records with specified capacity on the stack using alloca().
 *
 * When the batch is no longer needed, then use as_batch_records_destroy() to
 * release the batch and associated resources.
 *
 * @param __records		Batch record list.
 * @param __capacity	Initial capacity of batch record list. List will resize when necessary.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
#define as_batch_records_inita(__records, __capacity) \
	as_vector_inita(&((__records)->list), sizeof(as_batch_record), __capacity);

/**
 * Initialize batch records with specified capacity on the stack using alloca().
 *
 * @deprecated Use as_batch_records_inita() instead.
 * @relates as_batch_records
 * @ingroup batch_operations
 */
#define as_batch_read_inita(__records, __capacity) \
	as_vector_inita(&((__records)->list), sizeof(as_batch_record), __capacity);

/**
 * Initialize batch records with specified capacity on the heap.
 *
 * When the batch is no longer needed, then use as_batch_records_destroy() to
 * release the batch and associated resources.
 *
 * @param records	Batch record list.
 * @param capacity	Initial capacity of batch record list. List will resize when necessary.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline void
as_batch_records_init(as_batch_records* records, uint32_t capacity)
{
	as_vector_init(&records->list, sizeof(as_batch_record), capacity);
}

/**
 * Initialize batch records with specified capacity on the heap.
 *
 * @deprecated Use as_batch_records_init() instead.
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline void
as_batch_read_init(as_batch_records* records, uint32_t capacity)
{
	as_vector_init(&records->list, sizeof(as_batch_record), capacity);
}

/**
 * Create batch records on heap with specified list capacity on the heap.
 *
 * When the batch is no longer needed, then use as_batch_records_destroy() to
 * release the batch and associated resources.
 *
 * @param capacity	Initial capacity of batch record list. List will resize when necessary.
 * @return			Batch record list.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_records*
as_batch_records_create(uint32_t capacity)
{
	return (as_batch_records*)as_vector_create(sizeof(as_batch_record), capacity);
}

/**
 * Create batch records on heap with specified list capacity on the heap.
 *
 * @deprecated Use as_batch_records_create() instead.
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_records*
as_batch_read_create(uint32_t capacity)
{
	return (as_batch_records*)as_vector_create(sizeof(as_batch_record), capacity);
}

/**
 * Reserve a new `as_batch_read_record` slot.  Capacity will be increased when necessary.
 * Return reference to record.  The record is already initialized to zeroes.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_read_record*
as_batch_read_reserve(as_batch_records* records)
{
	as_batch_read_record* r = (as_batch_read_record*)as_vector_reserve(&records->list);
	r->type = AS_BATCH_READ;
	return r;
}

/**
 * Reserve a new `as_batch_write_record` slot.  Capacity will be increased when necessary.
 * Return reference to record.  The record is already initialized to zeroes.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_write_record*
as_batch_write_reserve(as_batch_records* records)
{
	as_batch_write_record* r = (as_batch_write_record*)as_vector_reserve(&records->list);
	r->type = AS_BATCH_WRITE;
	r->has_write = true;
	return r;
}

/**
 * Reserve a new `as_batch_apply_record` slot.  Capacity will be increased when necessary.
 * Return reference to record.  The record is already initialized to zeroes.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_apply_record*
as_batch_apply_reserve(as_batch_records* records)
{
	as_batch_apply_record* r = (as_batch_apply_record*)as_vector_reserve(&records->list);
	r->type = AS_BATCH_APPLY;
	r->has_write = true;
	return r;
}

/**
 * Reserve a new `as_batch_remove_record` slot.  Capacity will be increased when necessary.
 * Return reference to record.  The record is already initialized to zeroes.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline as_batch_remove_record*
as_batch_remove_reserve(as_batch_records* records)
{
	as_batch_remove_record* r = (as_batch_remove_record*)as_vector_reserve(&records->list);
	r->type = AS_BATCH_REMOVE;
	r->has_write = true;
	return r;
}

/**
 * Destroy keys and records in record list.  It's the responsility of the caller to 
 * free `as_batch_read_record.bin_names` and `as_batch_read_record.ops` when necessary.
 *
 * @relates as_batch_records
 * @ingroup batch_operations
 */
AS_EXTERN void
as_batch_records_destroy(as_batch_records* records);

/**
 * Destroy keys and records in record list.  It's the responsility of the caller to 
 * free `as_batch_read_record.bin_names` and `as_batch_read_record.ops` when necessary.
 *
 * @deprecated Use as_batch_records_destroy() instead.
 * @relates as_batch_records
 * @ingroup batch_operations
 */
static inline void
as_batch_read_destroy(as_batch_read_records* records)
{
	as_batch_records_destroy(records);
}

/**
 * Read multiple records for specified batch keys in one batch call.
 * This method allows different namespaces/bins to be requested for each key in the batch.
 * The returned records are located in the same batch array.
 * This method requires Aerospike Server version >= 3.6.0.
 *
 * ~~~~~~~~~~{.c}
 * as_batch_read_records records;
 * as_batch_read_inita(&records, 10);
 *
 * char* bin_names[] = {"bin1", "bin2"};
 * char* ns = "ns";
 * char* set = "set";
 *
 * as_batch_read_record* record = as_batch_read_reserve(&records);
 * as_key_init(&record->key, ns, set, "key1");
 * record->bin_names = bin_names;
 * record->n_bin_names = 2;
 *
 * record = as_batch_read_reserve(&records);
 * as_key_init(&record->key, ns, set, "key2");
 * record->read_all_bins = true;
 *
 * if (aerospike_batch_read(&as, &err, NULL, &records) != AEROSPIKE_OK) {
 *     fprintf(stderr, "error(%d) %s at [%s:%d]", err.code, err.message, err.file, err.line);
 * }
 *
 * as_batch_read_destroy(&records);
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param records		List of keys and bins to retrieve.
 * 						The returned records are located in the same array.
 *
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_read(
	aerospike* as, as_error* err, const as_policy_batch* policy, as_batch_read_records* records
	);

/**
 * Asynchronously read multiple records for specified batch keys in one batch call.
 * This method allows different namespaces/bins to be requested for each key in the batch.
 * The returned records are located in the same batch array.
 * This method requires Aerospike Server version >= 3.6.0.
 *
 * ~~~~~~~~~~{.c}
 * void my_listener(as_error* err, as_batch_read_records* records, void* udata, as_event_loop* event_loop)
 * {
 * 	   if (err) {
 * 	       fprintf(stderr, "Command failed: %d %s\n", err->code, err->message);
 * 	   }
 * 	   else {
 * 	       as_vector* list = &records->list;
 * 	       for (uint32_t i = 0; i < list->size; i++) {
 * 	           as_batch_read_record* record = as_vector_get(list, i);
 * 		       // Process record
 * 	       }
 *     }
 * 	   // Must free batch records on both success and error conditions because it was created
 * 	   // before calling aerospike_batch_read_async().
 * 	   as_batch_read_destroy(records);
 * }
 *
 * as_batch_read_records* records = as_batch_read_create(10);
 *
 * // bin_names must point to a static/global array of literal/global strings.
 * char* bin_names[] = {"bin1", "bin2"};
 * char* ns = "ns";
 * char* set = "set";
 *
 * as_batch_read_record* record = as_batch_read_reserve(records);
 * as_key_init(&record->key, ns, set, "key1");
 * record->bin_names = bin_names;
 * record->n_bin_names = 2;
 *
 * record = as_batch_read_reserve(records);
 * as_key_init(&record->key, ns, set, "key2");
 * record->read_all_bins = true;
 *
 * as_status status = aerospike_batch_read_async(&as, &err, NULL, records, NULL, my_listener, NULL);
 *
 * if (status != AEROSPIKE_OK) {
 * 	   // Must free batch records on queue error because the callback will not be called.
 * 	   as_batch_read_destroy(records);
 * }
 *
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param records		List of keys and bins to retrieve.  The returned records are located in the same array.
 * 						Must create using as_batch_read_create() (which allocates memory on heap) because 
 * 						async method will return immediately after queueing command.
 * @param listener 		User function to be called with command results.
 * @param udata 		User data to be forwarded to user callback.
 * @param event_loop 	Event loop assigned to run this command. If NULL, an event loop will be choosen by round-robin.
 *
 * @return AEROSPIKE_OK if async command succesfully queued. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_read_async(
	aerospike* as, as_error* err, const as_policy_batch* policy, as_batch_read_records* records,
	as_async_batch_listener listener, void* udata, as_event_loop* event_loop
	);

/**
 * Look up multiple records by key, then return all bins.
 *
 * ~~~~~~~~~~{.c}
 * as_batch batch;
 * as_batch_inita(&batch, 3);
 * 
 * as_key_init(as_batch_keyat(&batch,0), "ns", "set", "key1");
 * as_key_init(as_batch_keyat(&batch,1), "ns", "set", "key2");
 * as_key_init(as_batch_keyat(&batch,2), "ns", "set", "key3");
 * 
 * if (aerospike_batch_get(&as, &err, NULL, &batch, callback, NULL) != AEROSPIKE_OK) {
 * 	   fprintf(stderr, "error(%d) %s at [%s:%d]", err.code, err.message, err.file, err.line);
 * }
 *
 * as_batch_destroy(&batch);
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param batch			The batch of keys to read.
 * @param callback 		The callback to invoke for each record read.
 * @param udata			The user-data for the callback.
 *
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_get(
	aerospike* as, as_error* err, const as_policy_batch* policy, const as_batch* batch,
	aerospike_batch_read_callback callback, void* udata
	);

/**
 * @private
 * Perform batch reads for XDR.  The callback will be called for each record as soon as it's
 * received in no particular order.
 */
AS_EXTERN as_status
aerospike_batch_get_xdr(
	aerospike* as, as_error* err, const as_policy_batch* policy, const as_batch* batch,
	as_batch_callback_xdr callback, void* udata
	);

/**
 * Look up multiple records by key, then return specified bins.
 *
 * ~~~~~~~~~~{.c}
 * as_batch batch;
 * as_batch_inita(&batch, 3);
 *
 * as_key_init(as_batch_keyat(&batch,0), "ns", "set", "key1");
 * as_key_init(as_batch_keyat(&batch,1), "ns", "set", "key2");
 * as_key_init(as_batch_keyat(&batch,2), "ns", "set", "key3");
 *
 * const char* bin_filters[] = {"bin1", "bin2"};
 *
 * if (aerospike_batch_get_bins(&as, &err, NULL, &batch, bin_filters, 2, callback, NULL) != AEROSPIKE_OK ) {
 * 	   fprintf(stderr, "error(%d) %s at [%s:%d]", err.code, err.message, err.file, err.line);
 * }
 *
 * as_batch_destroy(&batch);
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param batch			The batch of keys to read.
 * @param bins			Bin filters.  Only return these bins.
 * @param n_bins		The number of bin filters.
 * @param callback 		The callback to invoke for each record read.
 * @param udata			The user-data for the callback.
 *
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_get_bins(
	aerospike* as, as_error* err, const as_policy_batch* policy, const as_batch* batch,
	const char** bins, uint32_t n_bins, aerospike_batch_read_callback callback, void* udata
	);

/**
 * Look up multiple records by key, then return results from specified read operations.
 *
 * ~~~~~~~~~~{.c}
 * as_batch batch;
 * as_batch_inita(&batch, 3);
 *
 * as_key_init(as_batch_keyat(&batch,0), "ns", "set", "key1");
 * as_key_init(as_batch_keyat(&batch,1), "ns", "set", "key2");
 * as_key_init(as_batch_keyat(&batch,2), "ns", "set", "key3");
 *
 * as_operations ops;
 * as_operations_inita(&ops, 1);
 * as_operations_list_size(&ops, "list", NULL);
 *
 * if (aerospike_batch_get_ops(&as, &err, NULL, &batch, &ops, callback, NULL) != AEROSPIKE_OK ) {
 * 	   fprintf(stderr, "error(%d) %s at [%s:%d]", err.code, err.message, err.file, err.line);
 * }
 *
 * as_batch_destroy(&batch);
 * as_operations_destroy(&ops);
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param batch			The batch of keys to read.
 * @param ops			Read operations.
 * @param callback 		The callback to invoke for each record read.
 * @param udata			The user-data for the callback.
 *
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_get_ops(
	aerospike* as, as_error* err, const as_policy_batch* policy, const as_batch* batch,
	as_operations* ops, aerospike_batch_read_callback callback, void* udata
	);

/**
 * Test whether multiple records exist in the cluster.
 *
 * ~~~~~~~~~~{.c}
 * as_batch batch;
 * as_batch_inita(&batch, 3);
 * 
 * as_key_init(as_batch_keyat(&batch,0), "ns", "set", "key1");
 * as_key_init(as_batch_keyat(&batch,1), "ns", "set", "key2");
 * as_key_init(as_batch_keyat(&batch,2), "ns", "set", "key3");
 * 
 * if (aerospike_batch_exists(&as, &err, NULL, &batch, callback, NULL) != AEROSPIKE_OK) {
 * 	   fprintf(stderr, "error(%d) %s at [%s:%d]", err.code, err.message, err.file, err.line);
 * }
 *
 * as_batch_destroy(&batch);
 * ~~~~~~~~~~
 *
 * @param as			The aerospike instance to use for this operation.
 * @param err			The as_error to be populated if an error occurs.
 * @param policy		The policy to use for this operation. If NULL, then the default policy will be used.
 * @param batch			The batch of keys to read.
 * @param callback 		The callback to invoke for each record read.
 * @param udata			The user-data for the callback.
 *
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 *
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_exists(
	aerospike* as, as_error* err, const as_policy_batch* policy, const as_batch* batch,
	aerospike_batch_read_callback callback, void* udata
	);

/**
 * Read/Write multiple records for specified batch keys in one batch call.
 * This method allows different sub-commands for each key in the batch.
 * The returned records are located in the same list.
 *
 * Requires server version 5.8+
 *
 * ~~~~~~~~~~{.c}
 * TODO: Provide code sample here from examples.
 * ~~~~~~~~~~
 *
 * @param as		Aerospike cluster instance.
 * @param err		Populated if an error occurs.
 * @param policy	Batch configuration parameters, pass in null for defaults.
 * @param records	List of batch sub-commands to perform.
 * 					The returned records are located in the same list.
 * @return AEROSPIKE_OK if successful. Otherwise an error.
 * @ingroup batch_operations
 */
AS_EXTERN as_status
aerospike_batch_operate(
	aerospike* as, as_error* err, const as_policy_batch* policy, as_batch_records* records
	);

// TODO: DOC
AS_EXTERN as_status
aerospike_batch_remove(
	aerospike* as, as_error* err, const as_policy_batch* policy,
	const as_policy_batch_remove* policy_remove, const as_batch* batch,
	aerospike_batch_callback callback, void* udata
	);

#ifdef __cplusplus
} // end extern "C"
#endif
