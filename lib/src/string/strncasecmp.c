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

#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

s32 strncasecmp(const char *str1,const char *str2,u32 count) {
	char c1,c2;
	s32 rem = count;
	vassert(str1 != NULL,"str1 == NULL");
	vassert(str2 != NULL,"str2 == NULL");

	c1 = tolower(*str1);
	c2 = tolower(*str2);
	while(c1 && c2 && rem-- > 0) {
		if(c1 != c2) {
			if(c1 < c2)
				return -1;
			return 1;
		}
		c1 = tolower(*++str1);
		c2 = tolower(*++str2);
	}
	if(rem <= 0)
		return 0;
	if(c1 && !c2)
		return 1;
	return -1;
}
