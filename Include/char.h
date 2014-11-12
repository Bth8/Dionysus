/* char.h - chardev driver management */

/* Copyright (C) 2014 Bth8 <bth8fwd@gmail.com>
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

#ifndef CHAR_H
#define CHAR_H

#include <common.h>
#include <vfs.h>

struct chrdev_driver {
	const char *name;
	struct file_ops ops;
};

void init_chardev(void);
struct chrdev_driver *get_chrdev_driver(uint32_t major);
int32_t register_chrdev(uint32_t major, const char *name, struct file_ops fops);

#endif /* CHAR_H */