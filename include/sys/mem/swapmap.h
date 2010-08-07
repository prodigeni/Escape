/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
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

#ifndef SWAPMAP_H_
#define SWAPMAP_H_

#include <sys/common.h>
#include <esc/sllist.h>

#define INVALID_BLOCK		0xFFFFFFFF

/**
 * Inits the swap-map
 *
 * @param swapSize the size of the swap-device in bytes
 */
void swmap_init(u32 swapSize);

/**
 * Allocates <count> continuous blocks on the swap-device
 *
 * @param count the number of pages to swap
 * @return the starting block on the swap-device or INVALID_BLOCK if no free space is left
 */
u32 swmap_alloc(u32 count);

/**
 * @param block the block-number
 * @return true if the given block is used
 */
bool swmap_isUsed(u32 block);

/**
 * Determines the free space in the swapmap
 *
 * @return the free space in bytes
 */
u32 swmap_freeSpace(void);

/**
 * Free's the given blocks
 *
 * @param block the starting block
 * @param count the number of blocks
 */
void swmap_free(u32 block,u32 count);


#if DEBUGGING

/**
 * Prints the swap-map
 */
void swmap_dbg_print(void);

#endif

#endif /* SWAPMAP_H_ */