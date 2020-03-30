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
#include "cio_server_socket.h"
#include "cio_util.h"

static struct cio_eventloop loop;

static const uint64_t close_timeout_ns = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
enum {BUFFER_SIZE = 100};

struct echo_client {
	struct cio_socket socket;
	uint8_t buffer[BUFFER_SIZE];
	struct cio_write_buffer wb;
	struct cio_write_buffer wbh;
	struct cio_read_buffer rb;
};

static struct cio_socket *alloc_echo_client(void)
{
	struct echo_client *client = malloc(sizeof(*client));
	if (cio_unlikely(client == NULL)) {
		return NULL;
	}

	return &client->socket;
}

static void free_echo_client(struct cio_socket *socket)
{
	struct echo_client *client = cio_container_of(socket, struct echo_client, socket);
	free(client);
}

static void handle_accept(struct cio_server_socket *ss, void *handler_context, enum cio_error err, struct cio_socket *socket)
{
	(void)handler_context;

	struct echo_client *client = cio_container_of(socket, struct echo_client, socket);
	(void)client;

	if (err != CIO_SUCCESS) {
		printk("accept error!\n");
		cio_server_socket_close(ss);
		cio_eventloop_cancel(ss->impl.loop);
		return;
	}

	//cio_read_buffer_init(&client->rb, client->buffer, sizeof(client->buffer));
	//struct cio_io_stream *stream = cio_socket_get_io_stream(socket);
	//stream->read_some(stream, &client->rb, handle_read, client);
}


void main(void)
{
	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		return -1;
	}
#if 0
	struct cio_server_socket ss;
	err = cio_server_socket_init(&ss, &loop, 5, alloc_echo_client, free_echo_client, close_timeout_ns, NULL);
	if (err != CIO_SUCCESS) {
		printk("error in cio_server_socket_init! %d\n", err);
		goto destroy_eventloop;
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
destroy_eventloop:
	cio_eventloop_destroy(&loop);
#endif
}
