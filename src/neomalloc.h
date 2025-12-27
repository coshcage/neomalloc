/*
 * Name:        neomalloc.h
 * Description: Neo malloc core function.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1207250701B1B07250708L00050
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
 
#ifndef _NEOMALLOC_H_
#define _NEOMALLOC_H_

#include <stddef.h>  /* Using type size_t. */
#include <stdbool.h> /* Boolean type and constants. */
 
/* Define constant NULL. */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Heap header structure. */
typedef struct st_HeapHeader
{
	size_t size;
	size_t hshsiz;
} HEAP_HEADER, * P_HEAP_HEADER;

/* Exported functions. */
P_HEAP_HEADER nmCreateHeap  (void *        pbase, size_t size,  size_t hshsiz);
P_HEAP_HEADER nmExtendHeap  (P_HEAP_HEADER ph,    size_t sizincl);
void *        nmAllocHeap   (P_HEAP_HEADER ph,    size_t size);
void          nmFreeHeap    (P_HEAP_HEADER ph,    void * ptr);
void *        nmReallocHeap (P_HEAP_HEADER ph,    void * ptr,   size_t size);
 
#endif

