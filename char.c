/* char.c - functions for creating and interacting with chardevs */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
 *
 *  This file is part of Dionysus.
 *
 *  Dionysus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Dionysus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dionysus.  If not, see <http://www.gnu.org/licenses/>
 */

#include <common.h>
#include <char.h>
#include <vfs.h>
#include <string.h>
#include <printf.h>

struct chrdev_driver char_drivers[256];		// 1 for every valid major number

void init_chardev(void) {
	int i;
	for (i = 0; i < 256; i++) {
		char_drivers[i].name = "Default";
		char_drivers[i].ops = (struct file_ops){ NULL };
	}
}

struct chrdev_driver *get_chrdev_driver(dev_t major) {
	if (major <= 0 || major > 256)
		return NULL;

	if (strcmp(char_drivers[major - 1].name, "Default") == 0)
		return NULL;

	return &char_drivers[major - 1];
}

spinlock_t char_lock = 0;

int32_t register_chrdev(dev_t major, const char *name, struct file_ops fops) {
	spin_lock(&char_lock);
	// Find an open major number if given zero
	if (major == 0) {
		// Max 256
		for (major = 1; strcmp(char_drivers[major - 1].name, "Default") != 0;
				major++)
			if (major == 256)
				return -1;
	}

	char_drivers[major - 1].name = name;
	char_drivers[major - 1].ops = fops;
	spin_unlock(&char_lock);

	printf("Chardev driver %s added\n", name);

	return major;
}