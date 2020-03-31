/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>

#include <zephyr.h>
#include <sys/printk.h>

#include "cio_error_code.h"
#include "cio_eventloop.h"
#include "cio_http_location_handler.h"
#include "cio_http_server.h"
#include "cio_util.h"
#include "cio_websocket_location_handler.h"
#include "cio_write_buffer.h"

static struct cio_eventloop loop;

enum {AUTOBAHN_SERVER_PORT = 9001};

enum {READ_BUFFER_SIZE = 10240};
enum {IPV4_ADDRESS_SIZE = 4};

static const uint64_t HEADER_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t BODY_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t RESPONSE_TIMEOUT = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t CLOSE_TIMEOUT_NS = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);

static struct cio_socket *alloc_http_client(void)
{
	struct cio_http_client *client = malloc(sizeof(*client) + READ_BUFFER_SIZE);
	if (cio_unlikely(client == NULL)) {
		return NULL;
	}

	client->buffer_size = READ_BUFFER_SIZE;
	return &client->socket;
}

static void free_http_client(struct cio_socket *socket)
{
	struct cio_http_client *client = cio_container_of(socket, struct cio_http_client, socket);
	free(client);
}

static void http_server_closed(struct cio_http_server *s)
{
	(void)s;
	cio_eventloop_cancel(&loop);
}

static void serve_error(struct cio_http_server *server, const char *reason)
{
	(void)reason;
	cio_http_server_shutdown(server, http_server_closed);
}

void main(void)
{
	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		printk("error in cio_eventloop_init! %d\n", err);
		return;
	}

	struct cio_http_server_configuration config = {
		.on_error = serve_error,
		.read_header_timeout_ns = HEADER_READ_TIMEOUT,
		.read_body_timeout_ns = BODY_READ_TIMEOUT,
		.response_timeout_ns = RESPONSE_TIMEOUT,
		.close_timeout_ns = CLOSE_TIMEOUT_NS,
		.alloc_client = alloc_http_client,
		.free_client = free_http_client
	};

	err = cio_init_inet_socket_address(&config.endpoint, cio_get_inet_address_any4(), AUTOBAHN_SERVER_PORT);
	if (err != CIO_SUCCESS) {
		printk("error in cio_init_inet_socket_address! %d\n", err);
		goto destroy_loop;
	}

	printk("started\n");

#if 0
	struct cio_server_socket ss;
	err = cio_server_socket_init(&ss, &loop, 5, alloc_echo_client, free_echo_client, close_timeout_ns, NULL);
	if (err != CIO_SUCCESS) {
		printk("error in cio_server_socket_init! %d\n", err);
		goto destroy_loop;
	}

	err = cio_server_socket_set_reuse_address(&ss, true);
	if (err != CIO_SUCCESS) {
		printk("error in cio_server_socket_set_reuse_address! %d\n", err);
		goto close_socket;
	}

	static const uint16_t SERVERSOCKET_LISTEN_PORT = 12345;
	err = cio_server_socket_bind(&ss, NULL, SERVERSOCKET_LISTEN_PORT);
	if (err != CIO_SUCCESS) {
		printk("error in cio_server_socket_bind! %d\n", err);
		goto close_socket;
	}

	err = cio_server_socket_accept(&ss, handle_accept, NULL);
	if (err != CIO_SUCCESS) {
		printk("error in cio_server_socket_accept! %d\n", err);
		goto close_socket;
	}

	k_sleep(5000);

	printk("closing the server socket! %d\n", err);
	cio_server_socket_close(&ss);

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		printk("error in cio_eventloop_run!\n");
		goto close_socket;
	}

close_socket:
	cio_server_socket_close(&ss);
#endif
destroy_loop:
	cio_eventloop_destroy(&loop);
}
