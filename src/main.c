/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>

#include <zephyr.h>
#include <misc/printk.h>

#include "cio_error_code.h"
#include "cio_eventloop.h"
#include "cio_timer.h"

static struct cio_eventloop loop;
static const uint64_t FIVE_S = 5000000000;
static uint_fast8_t expirations = 0;
static const uint_fast8_t MAX_EXPIRATIONS = 5;

static void handle_timeout(struct cio_timer *timer, void *handler_context, enum cio_error err)
{
	(void)handler_context;
	if (err == CIO_SUCCESS) {
		printk("timer expired!\n");
		if (expirations++ < MAX_EXPIRATIONS) {
			if (cio_timer_expires_from_now(timer, FIVE_S, handle_timeout, NULL) != CIO_SUCCESS) {
				printk("arming timer failed!\n");
			}
		} else {
			cio_timer_close(timer);
			cio_eventloop_cancel(&loop);
		}
	} else {
		printk("timer error!\n");
	}
}

int main(void)
{
	int ret = 0;
	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		return -1;
	}

	struct cio_timer timer;
	err = cio_timer_init(&timer, &loop, NULL);
	if (err != CIO_SUCCESS) {
		ret = -1;
		goto destroy_loop;
	}

	if (cio_timer_expires_from_now(&timer, FIVE_S, handle_timeout, NULL) != CIO_SUCCESS) {
		printk("arming timer failed!\n");
		ret = -1;
		goto destroy_loop;
	}

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		printk("error in cio_eventloop_run!\n");
	}

destroy_loop:
	cio_eventloop_destroy(&loop);

	return ret;

}
