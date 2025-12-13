/*
 * Name:        neomalloc.c
 * Description: Neo malloc core function.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1207250701C1207250908L00065
 * License:     LGPLv3
 * Copyright (C) 2025 John Cage
 *
 * This file is part of Neo Malloc.
 *
 * Neo Malloc is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Neo Malloc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with StoneValley.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "neomalloc.h"
#include <string.h>

#define SIZ 128

char buff[SIZ * 2];

int main()
{
	P_HEAP_HEADER ph;
	void * p1;

	memset(buff, 0xff, SIZ * 2);
	
	ph = nmCreateHeap(buff, SIZ, 7);
	
	if (NULL != ph)
	{
		ph = nmExtendHeap(ph, SIZ);
		
		if (NULL != ph)
		{
			p1 = nmAllocHeap(ph, 8);
			
			if (NULL != p1)
			{
				p1 = nmReallocHeap(ph, p1, 64);
				
				if (NULL != p1)
				{
					nmFreeHeap(ph, p1);
					return 0;
				}
				return 4;
			}
			return 3;
		}
		return 2;
	}
	return 1;
}

