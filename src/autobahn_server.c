/*
 * The MIT License (MIT)
 *
 * Copyright (c) <2017> <Stephan Gatzka>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cio_error_code.h"
#include "cio_eventloop.h"
#include "cio_http_location_handler.h"
#include "cio_http_server.h"
#include "cio_util.h"
#include "cio_websocket_location_handler.h"
#include "cio_write_buffer.h"

static struct cio_eventloop loop;
static struct cio_http_server http_server;

enum { AUTOBAHN_SERVER_PORT = 9001 };

enum { READ_BUFFER_SIZE = 10240 };
enum { IPV6_ADDRESS_SIZE = 16 };

static const uint64_t HEADER_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t BODY_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t RESPONSE_TIMEOUT = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t CLOSE_TIMEOUT_NS = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);

struct ws_autobahn_handler {
	struct cio_websocket_location_handler ws_handler;
	struct cio_write_buffer wbh;
	struct cio_write_buffer wb_message;
	size_t echo_write_index;
	bool start_new_write_chunk;
	uint8_t echo_buffer[READ_BUFFER_SIZE];
};

static void free_autobahn_handler(struct cio_websocket_location_handler *wslh)
{
	struct ws_autobahn_handler *h = cio_container_of(wslh, struct ws_autobahn_handler, ws_handler);
	free(h);
}

static void read_handler(struct cio_websocket *ws, void *handler_context, enum cio_error err, size_t frame_length, uint8_t *data, size_t length, bool last_chunk, bool last_frame, bool is_binary);

static void write_complete(struct cio_websocket *ws, void *handler_context, enum cio_error err)
{
	(void)handler_context;
	if (err == CIO_SUCCESS) {
		err = cio_websocket_read_message(ws, read_handler, NULL);
		if (err != CIO_SUCCESS) {
			fprintf(stderr, "could not start reading a new message!\n");
		}
	}
}

static void read_handler(struct cio_websocket *ws, void *handler_context, enum cio_error err, size_t frame_length, uint8_t *data, size_t length, bool last_chunk, bool last_frame, bool is_binary)
{
	(void)handler_context;

	if (err == CIO_SUCCESS) {
		struct cio_websocket_location_handler *handler = cio_container_of(ws, struct cio_websocket_location_handler, websocket);
		struct ws_autobahn_handler *ah = cio_container_of(handler, struct ws_autobahn_handler, ws_handler);

		if (frame_length <= READ_BUFFER_SIZE) {
			memcpy(ah->echo_buffer + ah->echo_write_index, data, length);
			ah->echo_write_index += length;
			if (last_chunk) {
				ah->echo_write_index = 0;
				cio_write_buffer_head_init(&ah->wbh);
				cio_write_buffer_const_element_init(&ah->wb_message, ah->echo_buffer, frame_length);
				cio_write_buffer_queue_tail(&ah->wbh, &ah->wb_message);
				cio_websocket_write_message_first_chunk(ws, cio_write_buffer_get_total_size(&ah->wbh), &ah->wbh, last_frame, is_binary, write_complete, NULL);
			} else {
				err = cio_websocket_read_message(ws, read_handler, NULL);
			}
		} else {
			// make chunked transfers
			cio_write_buffer_head_init(&ah->wbh);
			cio_write_buffer_const_element_init(&ah->wb_message, data, length);
			cio_write_buffer_queue_tail(&ah->wbh, &ah->wb_message);
			if (ah->start_new_write_chunk) {
				err = cio_websocket_write_message_first_chunk(ws, frame_length, &ah->wbh, last_frame, is_binary, write_complete, NULL);
			} else {
				err = cio_websocket_write_message_continuation_chunk(ws, &ah->wbh, write_complete, NULL);
			}

			ah->start_new_write_chunk = last_chunk;
		}

		if (err != CIO_SUCCESS) {
			fprintf(stderr, "could not start writing message!\n");
		}

	} else if (err != CIO_EOF) {
		fprintf(stderr, "read failure!\n");
	}
}

static void on_connect(struct cio_websocket *ws)
{
	enum cio_error err = cio_websocket_read_message(ws, read_handler, NULL);
	if (err != CIO_SUCCESS) {
		fprintf(stderr, "could not start reading a new message!\n");
	}
}

static void on_error(const struct cio_websocket *ws, enum cio_error err, const char *reason)
{
	(void)ws;
	fprintf(stderr, "Unexpected error: %d, %s\n", err, reason);
}

static struct cio_http_location_handler *alloc_autobahn_handler(const void *config)
{
	(void)config;
	struct ws_autobahn_handler *handler = malloc(sizeof(*handler));
	if (cio_unlikely(handler == NULL)) {
		return NULL;
	}

	enum cio_error err = cio_websocket_location_handler_init(&handler->ws_handler, NULL, 0, on_connect, free_autobahn_handler);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		free(handler);
		return NULL;
	}

	cio_websocket_set_on_error_cb(&handler->ws_handler.websocket, on_error);
	handler->echo_write_index = 0;
	handler->start_new_write_chunk = true;
	return &handler->ws_handler.http_location;
}

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

static void http_server_closed(const struct cio_http_server *s)
{
	(void)s;
	cio_eventloop_cancel(&loop);
}

static void serve_error(struct cio_http_server *server, const char *reason)
{
	(void)reason;
	cio_http_server_shutdown(server, http_server_closed);
}

void  main(void)
{
	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		fprintf(stderr, "Error in cio_eventloop_init\n");
		return;
	}

	struct cio_http_server_configuration config = {
	    .on_error = serve_error,
	    .read_header_timeout_ns = HEADER_READ_TIMEOUT,
	    .read_body_timeout_ns = BODY_READ_TIMEOUT,
	    .response_timeout_ns = RESPONSE_TIMEOUT,
	    .close_timeout_ns = CLOSE_TIMEOUT_NS,
	    .alloc_client = alloc_http_client,
	    .free_client = free_http_client,
	    .use_tcp_fastopen = false};

	err = cio_init_inet_socket_address(&config.endpoint, cio_get_inet_address_any4(), AUTOBAHN_SERVER_PORT);
	if (err != CIO_SUCCESS) {
		fprintf(stderr, "Error in cio_init_inet_socket_address\n");
		goto destroy_loop;
	}

	err = cio_http_server_init(&http_server, &loop, &config);
	if (err != CIO_SUCCESS) {
		fprintf(stderr, "Error in cio_http_server_init\n");
		goto destroy_loop;
	}

	struct cio_http_location autobahn_target;
	cio_http_location_init(&autobahn_target, "/", NULL, alloc_autobahn_handler);
	cio_http_server_register_location(&http_server, &autobahn_target);

	err = cio_http_server_serve(&http_server);
	if (err != CIO_SUCCESS) {
		fprintf(stderr, "Error in cio_http_server_serve\n");
		goto destroy_loop;
	}

	cio_eventloop_run(&loop);

destroy_loop:
	cio_eventloop_destroy(&loop);
	return;
}
