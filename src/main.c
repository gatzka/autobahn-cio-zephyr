/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

#include <cio_version.h>

void main(void)
{
	printk("Using cio version: %s\n", cio_get_version_string());
}
