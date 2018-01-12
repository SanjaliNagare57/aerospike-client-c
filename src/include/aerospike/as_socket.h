/*
 * Copyright 2008-2018 Aerospike, Inc.
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

#include <aerospike/as_address.h>
#include <aerospike/as_error.h>
#include <citrusleaf/cf_clock.h>
#include <pthread.h>
#include <stddef.h>

#if !defined(_MSC_VER)
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>

#define as_socket_fd int
#define as_socket_data_t void
#define as_socket_size_t size_t
#define AS_CONNECTING EINPROGRESS
#define AS_WOULDBLOCK EWOULDBLOCK
#define as_close(_fd) close((_fd))
#define as_last_error() errno

#if defined(__APPLE__)
#define SOL_TCP IPPROTO_TCP
#define MSG_NOSIGNAL 0
#endif

#else // _MSC_VER
#define as_socket_fd SOCKET
#define AS_CONNECTING WSAEWOULDBLOCK
#define AS_WOULDBLOCK WSAEWOULDBLOCK
#define SHUT_RDWR SD_BOTH
#define as_close(_fd) closesocket((_fd))
#define as_last_error() WSAGetLastError()
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ssl_ctx_st;

/**
 * This structure holds TLS context which can be shared (read-only)
 * by all the connections to a specific cluster.
 */
typedef struct as_tls_context_s {
	pthread_mutex_t lock;
	struct ssl_ctx_st* ssl_ctx;
	void* cert_blacklist;
	bool log_session_info;
} as_tls_context;

struct as_conn_pool_lock_s;
struct as_node_s;

/**
 * Socket fields for both regular and TLS sockets.
 */
typedef struct as_socket_s {
#if !defined(_MSC_VER)
	int fd;
	int family;
#else
	SOCKET fd;
#endif
	union {
		struct as_conn_pool_lock_s* pool_lock; // Used when sync socket is active.
		struct {
			uint32_t max_socket_idle;
			uint32_t last_used;
		} idle_check;                          // Used when socket in pool.
	};
	as_tls_context* ctx;
	const char* tls_name;
	struct ssl_st* ssl;
} as_socket;

/**
 * @private
 * Initialize an as_socket structure.
 */
void
as_socket_init(as_socket* sock);
	
/**
 * @private
 * Create non-blocking socket.  Family should be AF_INET or AF_INET6.
 * Return zero on success.
 */
int
as_socket_create_fd(int family, as_socket_fd* fdp);

/**
 * @private
 * Create non-blocking socket.
 * Family should be AF_INET or AF_INET6.
 * Return zero on success.
 */
int
as_socket_create(as_socket* sock, int family, as_tls_context* ctx, const char* tls_name);

/**
 * @private
 * Wrap existing fd in a socket.
 * Family should be AF_INET or AF_INET6.
 */
bool
as_socket_wrap(as_socket* sock, int family, as_socket_fd fd, as_tls_context* ctx, const char* tls_name);

/**
 * @private
 * Connect to non-blocking socket.
 */
static inline bool
as_socket_connect_fd(as_socket_fd fd, struct sockaddr* addr, socklen_t size)
{
	return connect(fd, addr, size) == 0 || as_last_error() == AS_CONNECTING;
}

/**
 * @private
 * Connect to non-blocking socket and perform sync TLS connect if TLS enabled.
 */
bool
as_socket_start_connect(as_socket* sock, struct sockaddr* addr, uint64_t deadline_ms);

/**
 * @private
 * Create non-blocking socket and connect.
 */
as_status
as_socket_create_and_connect(as_socket* sock, as_error* err, struct sockaddr* addr, as_tls_context* ctx, const char* tls_name, uint64_t deadline_ms);

/**
 * @private
 * Close and release resources associated with a as_socket.
 */
void
as_socket_close(as_socket* sock);

/**
 * @private
 * Create error message for socket error.
 */
as_status
as_socket_error(as_socket_fd fd, struct as_node_s* node, as_error* err, as_status status, const char* msg, int code);

/**
 * @private
 * Append address to error message.
 */
void
as_socket_error_append(as_error* err, struct sockaddr* addr);

/**
 * @private
 * Peek for socket connection status using underlying fd.
 *  Needed to support libuv.
 *
 * @return   0 : socket is connected, but no data available.
 * 		> 0 : byte size of data available.
 * 		< 0 : socket is invalid.
 */
int
as_socket_validate_fd(as_socket_fd fd);

/**
 * @private
 * Peek for socket connection status.
 *
 * @return   0 : socket is connected, but no data available.
 * 		> 0 : byte size of data available.
 * 		< 0 : socket is invalid.
 */
int
as_socket_validate(as_socket* sock);

/**
 * @private
 * Calculate future deadline given timeout.
 */
static inline uint64_t
as_socket_deadline(uint32_t timeout_ms)
{
	return (timeout_ms && timeout_ms <= INT32_MAX)? cf_getms() + timeout_ms : 0;
}

/**
 * @private
 * Write socket data with future deadline in milliseconds.
 * If deadline is zero, do not set deadline.
 */
as_status
as_socket_write_deadline(
	as_error* err, as_socket* sock, struct as_node_s* node, uint8_t *buf, size_t buf_len,
	uint32_t socket_timeout, uint64_t deadline
	);

/**
 * @private
 * Read socket data with future deadline in milliseconds.
 * If deadline is zero, do not set deadline.
 */
as_status
as_socket_read_deadline(
	as_error* err, as_socket* sock, struct as_node_s* node, uint8_t *buf, size_t buf_len,
	uint32_t socket_timeout, uint64_t deadline
	);

#ifdef __cplusplus
} // end extern "C"
#endif
