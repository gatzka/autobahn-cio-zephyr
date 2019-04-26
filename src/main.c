/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr.h>
#include <misc/printk.h>

#include <cio_random.h>
#include <cio_version.h>

void main(void)
{
	printk("Using cio version: %s\n", cio_get_version_string());
	uint8_t random_buffer[4];
	cio_random_get_bytes(random_buffer, sizeof(random_buffer));
	printk("randoms: %x %x %x %x\n", random_buffer[0], random_buffer[1], random_buffer[2], random_buffer[3]);
}
