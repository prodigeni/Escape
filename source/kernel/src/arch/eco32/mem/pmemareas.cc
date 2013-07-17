/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/common.h>
#include <sys/mem/pmemareas.h>
#include <sys/mem/pmem.h>
#include <sys/mem/paging.h>
#include <sys/boot.h>
#include <sys/util.h>

void pmemareas_initArch(void) {
	const sBootInfo *binfo = boot_getInfo();
	const sLoadProg *last;

	/* mark everything behind the modules as free */
	if(binfo->progCount == 0)
		util_panic("No boot-modules found");
	last = binfo->progs + binfo->progCount - 1;
	pmemareas_add(ROUND_PAGE_UP(last->start - KERNEL_START + last->size),binfo->memSize);
}

size_t pmemareas_getTotal(void) {
	const sBootInfo *binfo = boot_getInfo();
	return binfo->memSize;
}