/*
 * Name:        neomalloc.c
 * Description: Neo malloc core function.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1207250701A1207250800L00651
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
#include <limits.h>  /* Using macro CHAR_BIT. */
#include <string.h>  /* Using function memcpy, memset. */

#ifdef _MSC_VER
#include <intrin.h>  /* Use function __lzcnt16, __lzcnt and __lzcnt64. */
#endif

/* [HEAP MEMORY DIAGRAM]
 * +=HEAP_HEADER=+
 * |size         *===>sizeof(chunk) == head note + free chunk + data + foot note.
 * |-------------|
 * |hshsiz:3     |
 * +=============+
 * |   POINTER   *>-------\    Big chunks.
 * |-------------|        V
 * |   POINTER   *-->NULL |
 * |-------------|        |
 * |   POINTER   *-->NULL |    Small chunks.
 * +=============+--------|--------\
 * |Head_note    |        V        |
 * +=FREE_CHUNK==+<-------/<--\    |
 * | p[FCP_PREV] *>-----------/    |
 * |-------------|            ^    |
 * | p[FCP_NEXT] *>-----------/    > This is a free chunk.
 * +=============+                 |
 * |    DATA     *==sizeof(size_t) |
 * |             |  *4             |
 * |             |                 |
 * |             |                 |
 * +=============+        /--------/
 * |Foot_note    |        |
 * +=============+--------/
 */

/* sizeof(UCHART) == 1. */
typedef unsigned char * PUCHAR;
typedef unsigned char   UCHART;

/* Chunk pointer indices. */
enum en_FreeChunkPointer
{
	FCP_PREV = 0,
	FCP_NEXT = 1,
	FCP_MAX = 2
};

/* Used and unused mark. */
#define USED false
#define FREE true

/* Free chunk linked list node. */
typedef struct st_FreeChunk
{
	struct st_FreeChunk * p[FCP_MAX];
} FREE_CHUNK, * P_FREE_CHUNK;

/* Usable macros. */
#define MIN_CHUNK_SIZE (sizeof(size_t) * 2 + sizeof(FREE_CHUNK))
#define HEAP_BEGIN(ph) (sizeof(HEAP_HEADER) + (((P_HEAP_HEADER)(ph))->hshsiz * sizeof(P_HEAP_HEADER)))
#define ALIGN          (sizeof(size_t) * CHAR_BIT / 4)
#define MASK           (ALIGN - 1)
#define ASIZE(size)    (((size) & MASK) ? ((size) + ALIGN) & ~(size_t)MASK: (size))
#define HEAD_NOTE(pfc) (*(size_t *)((PUCHAR)(pfc) - sizeof(size_t)))
#define FOOT_NOTE(pfc) (*(size_t *)((PUCHAR)(pfc) + (HEAD_NOTE(pfc) & ~(size_t)MASK)))
#define FREE_MASK      ( (size_t)FREE)
#define USED_MASK      (~(size_t)USED - 1)

/* File level function declarations. */
size_t         _nmCLZ             (size_t n);
P_FREE_CHUNK * _nmLocateHashTable (P_HEAP_HEADER ph, size_t i);
void           _nmUnlinkChunk     (P_HEAP_HEADER ph, P_FREE_CHUNK ptr);
void           _nmRestoreEntrance (P_FREE_CHUNK * ppfc, P_FREE_CHUNK pfc);
void *         _nmSplitChunk      (P_HEAP_HEADER ph, P_FREE_CHUNK pfc, size_t size);
void           _nmPutChunk        (P_HEAP_HEADER ph, P_FREE_CHUNK pfc);

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmCLZ
 * Description:   Count leading zero for size_t integer.
 * Parameter:
 *         n The integer you wan to count.
 * Return value:  Integer of leading zeros.
 */
size_t _nmCLZ(size_t n)
{
#ifdef __GNUC__
	if (sizeof(size_t) == sizeof(long))
		return __builtin_clzl(n);
	else if (sizeof(size_t) == sizeof(long long))
		return __builtin_clzll(n);
	else
		return __builtin_clz(n);
#elif defined _MSC_VER
	switch (sizeof(size_t) * CHAR_BIT)
	{
	case 16:
		return __lzcnt16((short)n);
	case 32:
		return __lzcnt(n);
#ifdef _M_X64
	case 64:
		return __lzcnt64(n);
#endif
	}
#endif
	register size_t i = n, j = 0;
	while (i)
	{
		i >>= 1;
		++j;
	}
	return sizeof(size_t) * CHAR_BIT - j;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmLocateHashTable
 * Description:   Locate into hash table by given index.
 * Parameters:
 *         ph Pointer to heap header.
 *          i Index.
 * Return value:  Pointer to hash table content.
 */
P_FREE_CHUNK * _nmLocateHashTable(P_HEAP_HEADER ph, size_t i)
{
	if (ph->hshsiz > i) /* Locate into hash table directly. */
		return &i[(P_FREE_CHUNK *)((PUCHAR)ph + sizeof(HEAP_HEADER))];
	else /* Locate to biggest one. */
		return &(ph->hshsiz - 1)[(P_FREE_CHUNK *)((PUCHAR)ph + sizeof(HEAP_HEADER))];
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmUnlinkChunk
 * Description:   Cut chunk off from linked list.
 * Parameters:
 *         ph Pointer to heap header.
 *        ptr Pointer to a free chunk.
 * Return value:  N/A.
 */
void _nmUnlinkChunk(P_HEAP_HEADER ph, P_FREE_CHUNK ptr)
{
	register P_FREE_CHUNK pfc = ptr, pofc = pfc;
	register P_FREE_CHUNK * ppfc;

	if ((_nmCLZ(HEAD_NOTE(pfc) & ~(size_t)MASK)) - _nmCLZ(ph->size) <= ph->hshsiz)
	{
		ppfc = _nmLocateHashTable(ph, _nmCLZ(HEAD_NOTE(pfc)) - _nmCLZ(ph->size));

		if (NULL != *ppfc)
		{
			pfc = *ppfc;
			do
			{
				pfc = pfc->p[FCP_PREV];
				if (ptr == pfc)
				{
					register size_t i;

					pfc->p[FCP_PREV]->p[FCP_NEXT] = pfc->p[FCP_NEXT];
					pfc->p[FCP_NEXT]->p[FCP_PREV] = pfc->p[FCP_PREV];

					i = _nmCLZ(HEAD_NOTE(pfc)) - _nmCLZ(ph->size);
					if (pfc == *_nmLocateHashTable(ph, i)) /* Reach at linked list header.. */
						*_nmLocateHashTable(ph, i) = NULL;

					break;
				}
			} while (pfc != pofc);
		}
	}
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmRestoreEntrance
 * Description:   Restore hash table entrance.
 * Parameters:
 *       ppfc Pointer to hash table entrance.
 *        pfc Pointer to a free chunk.
 * Return value:  N/A.
 */
void _nmRestoreEntrance(P_FREE_CHUNK * ppfc, P_FREE_CHUNK pfc)
{
	pfc->p[FCP_PREV] = pfc->p[FCP_NEXT] = pfc;
	if (NULL != *ppfc)
	{
		pfc->p[FCP_NEXT] = *ppfc;
		(*ppfc)->p[FCP_PREV]->p[FCP_NEXT] = pfc;
		pfc->p[FCP_PREV] = (*ppfc)->p[FCP_PREV]->p[FCP_PREV];
		(*ppfc)->p[FCP_PREV]->p[FCP_PREV] = pfc;
	}
	*ppfc = pfc;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmSplitChunk
 * Description:   Split one chunk to two chunks and put back the latter one.
 * Parameters:
 *         ph Pointer to heap header.
 *        pfc Pointer to a chunk to be split.
 *       size Size of the former chunk.
 * Return value:  The same chunk which is the same to pfc.
 */
void * _nmSplitChunk(P_HEAP_HEADER ph, P_FREE_CHUNK pfc, size_t size)
{
	register void * pt;
	register size_t i;

	i = HEAD_NOTE(pfc) - size - sizeof(size_t);
	i &= ~(size_t)MASK;
	HEAD_NOTE(pfc) = size;
	FOOT_NOTE(pfc) = size;
	pt = pfc;
	pfc = (P_FREE_CHUNK)((PUCHAR)pfc + size + sizeof(size_t) * 2);
	HEAD_NOTE(pfc) = i;
	FOOT_NOTE(pfc) = i;
	HEAD_NOTE(pfc) |= FREE_MASK;
	FOOT_NOTE(pfc) |= FREE_MASK;

	/* Search hash table and put new free chunk to the linked list. */
	_nmRestoreEntrance(_nmLocateHashTable(ph, _nmCLZ(i) - _nmCLZ(ph->size)), pfc);

	return pt;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _nmPutChunk
 * Description:   Put free chunk back to linked list.
 * Parameters:
 *         ph Pointer to heap header.
 *        pfc Pointer to a free chunk.
 * Return value:  N/A.
 */
void _nmPutChunk(P_HEAP_HEADER ph, P_FREE_CHUNK pfc)
{
	if ((_nmCLZ(HEAD_NOTE(pfc) & ~(size_t)MASK)) - _nmCLZ(ph->size) > ph->hshsiz)
	{
		HEAD_NOTE(pfc) |= FREE_MASK;
		FOOT_NOTE(pfc) |= FREE_MASK;
	}
	else
		_nmRestoreEntrance(_nmLocateHashTable(ph, _nmCLZ(HEAD_NOTE(pfc)) - _nmCLZ(ph->size)), pfc);
}

/* Function name: nmCreateHeap
 * Description:   Create a heap from a memory buffer.
 * Parameters:
 *      pbase Memory buffer starting address.
 *       size Size of the whole buffer.(Unit in byte)
 *     hshsiz Count of hash table entrances.
 * Return value:  NULL: Failed.
 *                Pointer to heap header(same as pbase): Succeeded.
 * Tip:           Parameter size must be greater than or equal to (sizeof(HEAP_HEADER) + (hshsiz * sizeof(P_FREE_CHUNK)) + MIN_CHUNK_SIZE).
 */
P_HEAP_HEADER nmCreateHeap(void * pbase, size_t size, size_t hshsiz)
{
	P_FREE_CHUNK pfc;
	HEAP_HEADER hh;
	FREE_CHUNK fc;
	size_t t;

	if (NULL == pbase)
		return NULL;

	if (0 == hshsiz)
		return NULL;

	if (size < sizeof(HEAP_HEADER) + (hshsiz * sizeof(P_FREE_CHUNK)) + MIN_CHUNK_SIZE)
		return NULL;

	hh.size = size - (sizeof(HEAP_HEADER) + (hshsiz * sizeof(P_FREE_CHUNK)));
	hh.size &= ~(size_t)MASK;
	hh.hshsiz = hshsiz;

	/* Set heap header. */
	memcpy(pbase, &hh, sizeof(HEAP_HEADER));

	/* Clear hash table. */
	memset((PUCHAR)pbase + sizeof(HEAP_HEADER), 0, hshsiz * sizeof(size_t));

	/* Set one free chunk. */
	t = hh.size - (2 * sizeof(size_t));
	if (t > ASIZE(t))
		t = ASIZE(t);
	else
		t = t & ~(size_t)MASK;

	pfc = (P_FREE_CHUNK)((PUCHAR)pbase + HEAP_BEGIN(&hh) + sizeof(size_t));
	HEAD_NOTE(pfc) = t;
	FOOT_NOTE(pfc) = t;
	HEAD_NOTE(pfc) |= FREE_MASK;
	FOOT_NOTE(pfc) |= FREE_MASK;

	/* Set free chunk structure. */
	fc.p[FCP_PREV] = fc.p[FCP_NEXT] = pfc;
	memcpy(pfc, &fc, sizeof(FREE_CHUNK));

	/* Set hash table. */
	*_nmLocateHashTable(pbase, _nmCLZ(t) - _nmCLZ(hh.size)) = pfc;

	return (P_HEAP_HEADER)pbase;
}

/* Function name: nmCreateHeap
 * Description:   Enlarge heap.
 * Parameters:
 *         ph Pointer to heap header.
 *    sizincl The incremental you want to extend.(Unit in byte)
 * Return value:  NULL: Failed.
 *                Pointer to heap header(same as ph): Succeeded.
 * Tip:           Parameter sizincl must be greater than or equal to (MIN_CHUNK_SIZE).
 */
P_HEAP_HEADER nmExtendHeap(P_HEAP_HEADER ph, size_t sizincl)
{
	if (sizincl < MIN_CHUNK_SIZE)
		return NULL;
	else
	{
		/* Get the last chunk. */
		bool bused;
		size_t i;
		PUCHAR phead;
		P_FREE_CHUNK pfc;

		phead = (PUCHAR)ph + HEAP_BEGIN(ph);
		phead += ph->size - sizeof(size_t);

		i = *(size_t *)phead;
		bused = !!(i & ~(size_t)MASK);

		if (FREE != bused)
		{
			pfc = (P_FREE_CHUNK)(phead + sizeof(size_t) * 2);

			sizincl &= ~(size_t)MASK;
			ph->size += sizincl;
			sizincl -= sizeof(size_t) * 2;

			HEAD_NOTE(pfc) = sizincl;
			FOOT_NOTE(pfc) = sizincl;
			HEAD_NOTE(pfc) |= FREE_MASK;
			FOOT_NOTE(pfc) |= FREE_MASK;

			_nmPutChunk(ph, pfc);
		}
		else
		{
			phead -= (i & ~(size_t)MASK);
			pfc = (P_FREE_CHUNK)phead;

			sizincl += i;
			sizincl &= ~(size_t)MASK;

			_nmUnlinkChunk(ph, pfc);
			
			ph->size = sizincl;

			HEAD_NOTE(pfc) = sizincl;
			FOOT_NOTE(pfc) = sizincl;
			HEAD_NOTE(pfc) |= FREE_MASK;
			FOOT_NOTE(pfc) |= FREE_MASK;

			_nmPutChunk(ph, pfc);
		}

		return ph;
	}
}

/* Function name: nmAllocHeap
 * Description:   Heap allocation.
 * Parameters:
 *         ph Pointer to heap header.
 *       size Size in bytes you want to allocate.
 * Return value:  NULL: Failed.
 *                Pointer to a heap address: Succeeded.
 */
void * nmAllocHeap(P_HEAP_HEADER ph, size_t size)
{
	register size_t i, j, k;
	register P_FREE_CHUNK pfc, * ppfc;
	
	if (0 == size)
		size = ALIGN;

	size = ASIZE(size);
	j = _nmCLZ(size) - _nmCLZ(ph->size);

	/* Definitely cannot allocate. */
	if (j > _nmCLZ(size))
		return NULL;

	/* Search hash table. */
	ppfc = _nmLocateHashTable(ph, j);
	pfc = *ppfc;

	k = ppfc - (P_FREE_CHUNK *)((PUCHAR)ph + sizeof(HEAP_HEADER));

	for (i = 0; i < k; ++i)
	{
		if (NULL != pfc)
			break;
		--ppfc;
		pfc = *ppfc;
	}
	
	/* Search for fit chunk. */
	if (NULL != pfc)
	{
		register P_FREE_CHUNK pofc = pfc;
		do
		{
			if (size > (HEAD_NOTE(pfc) & ~(size_t)MASK))
				pfc = pfc->p[FCP_NEXT];
			else
				break;
		} while (pfc != pofc);
	}
	else
		return NULL; /* No available space. */

	if (size > (HEAD_NOTE(pfc) & ~(size_t)MASK))
		return NULL; /* Cannot find a suitable chunk. */
	else /* Cut one chunk. */
	{
		if (size == (HEAD_NOTE(pfc) & ~(size_t)MASK) || size > (HEAD_NOTE(pfc) & ~(size_t)MASK) - MIN_CHUNK_SIZE)
		{	/* No need to split. */
			_nmUnlinkChunk(ph, pfc);

			/* Set used. */
			HEAD_NOTE(pfc) &= USED_MASK;
			FOOT_NOTE(pfc) &= USED_MASK;

			return pfc;
		}
		else
		{
			_nmUnlinkChunk(ph, pfc);
			return _nmSplitChunk(ph, pfc, size); /* Split. */
		}
	}
}

/* Function name: nmFreeHeap
 * Description:   Heap freedom.
 * Parameters:
 *         ph Pointer to heap header.
 *        ptr Pointer in heap that you want to free.
 * Return value:  N/A.
 */
void nmFreeHeap(P_HEAP_HEADER ph, void * ptr)
{
	register P_FREE_CHUNK pfc = (P_FREE_CHUNK)ptr;
	register bool bbtm = FREE, bhed = FREE;
	register size_t upcolsiz, upcolcnt;
	register size_t lwcolsiz, lwcolcnt;
	register size_t chksiz;
	register size_t i;
	
	if (NULL == ptr)
		return;

	/* Get upper bound. */
	if ((PUCHAR)pfc - sizeof(size_t) == (PUCHAR)ph + HEAP_BEGIN(ph))
		bhed = USED;

	upcolsiz = 0;
	upcolcnt = 0;

	do
	{
		if (FREE == bhed)
		{
			register size_t t = *(size_t *)((PUCHAR)pfc - 2 * sizeof(size_t));
			chksiz = (t & ~(size_t)MASK);
			if (FREE == !!(t & (size_t)MASK))
			{
				pfc = (P_FREE_CHUNK)((PUCHAR)pfc - 2 * sizeof(size_t) - chksiz);
				upcolsiz += chksiz;
				++upcolcnt;

				/* Unlink chunk. */
				_nmUnlinkChunk(ph, pfc);
			}
			else
				break;
		}
		else
			break;

		if ((PUCHAR)pfc - sizeof(size_t) == (PUCHAR)ph + HEAP_BEGIN(ph))
			bhed = USED;
	} while (USED != bhed);

	pfc = (P_FREE_CHUNK)ptr;

	/* Get lower bound. */
	chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;

	if ((PUCHAR)pfc + chksiz + sizeof(size_t) - ((PUCHAR)ph + HEAP_BEGIN(ph)) == ph->size)
		bbtm = USED;

	lwcolsiz = 0;
	lwcolcnt = 0;

	if (FREE == bbtm)
	{
		while (FREE == !!(*(size_t *)((PUCHAR)pfc + chksiz + sizeof(size_t)) & (size_t)MASK))
		{
			pfc = (P_FREE_CHUNK)((PUCHAR)pfc + chksiz + 2 * sizeof(size_t));
			chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;
			lwcolsiz += chksiz;
			++lwcolcnt;

			/* Unlink chunk. */
			_nmUnlinkChunk(ph, pfc);
		}
	}

	/* Coalesce up and downward. */
	pfc = (P_FREE_CHUNK)ptr;

	chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;

	i = upcolsiz + sizeof(size_t) * 2 * upcolcnt;

	if (upcolcnt)
		pfc = (P_FREE_CHUNK)((PUCHAR)pfc - i);

	chksiz += i;

	chksiz += lwcolsiz + sizeof(size_t) * 2 * lwcolcnt;

	HEAD_NOTE(pfc) = chksiz;
	FOOT_NOTE(pfc) = chksiz;
	HEAD_NOTE(pfc) |= FREE_MASK;
	FOOT_NOTE(pfc) |= FREE_MASK;

	/* Put free chunk back into hash table or leave it alone. */
	_nmPutChunk(ph, pfc);
}

/* Function name: nmReallocHeap
 * Description:   Heap reallocation.
 * Parameters:
 *         ph Pointer to heap header.
 *        ptr Pointer in heap that you want to reallocate.
 *       size New size of memory chunk.
 * Return value:  NULL: Reallocation failed.
 *                Pointer to heap memory: Succeeded.
 * Tip:           The return value can either be ptr or a new address or NULL.
 */
void * nmReallocHeap(P_HEAP_HEADER ph, void * ptr, size_t size)
{
	register size_t i;
	register size_t chksiz;
	register size_t lwcolsiz, lwcolcnt;
	register bool bbtm = FREE;

	if (NULL == ptr)
		return nmAllocHeap(ph, size);

	size = ASIZE(size);
	i = _nmCLZ(size) - _nmCLZ(ph->size);

	/* Definitely cannot allocate. */
	if (i > _nmCLZ(size))
		return NULL;

	if (size < HEAD_NOTE((P_FREE_CHUNK)ptr))
		return _nmSplitChunk(ph, (P_FREE_CHUNK)ptr, size); /* Split. */
	else if (size == HEAD_NOTE((P_FREE_CHUNK)ptr))
		return ptr;
	else /* Try to collect residues. */
	{
		register P_FREE_CHUNK pfc;

		pfc = (P_FREE_CHUNK)ptr;

		/* Get lower bound. */
		chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;

		if ((PUCHAR)pfc + chksiz + sizeof(size_t) - ((PUCHAR)ph + HEAP_BEGIN(ph)) == ph->size)
			bbtm = USED;

		lwcolsiz = 0;
		lwcolcnt = 0;

		if (FREE == bbtm)
		{
			while (FREE == !!(*(size_t *)((PUCHAR)pfc + chksiz + sizeof(size_t)) & (size_t)MASK))
			{
				pfc = (P_FREE_CHUNK)((PUCHAR)pfc + chksiz + 2 * sizeof(size_t));
				chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;
				lwcolsiz += chksiz;
				++lwcolcnt;

				/* Unlink chunk. */
				_nmUnlinkChunk(ph, pfc);
			}
		}

		/* Coalesce downward. */
		pfc = (P_FREE_CHUNK)ptr;

		chksiz = HEAD_NOTE(pfc) & ~(size_t)MASK;

		chksiz += lwcolsiz + sizeof(size_t) * 2 * lwcolcnt;

		HEAD_NOTE(pfc) = chksiz;
		FOOT_NOTE(pfc) = chksiz;

		if (chksiz >= size)
		{
			HEAD_NOTE(pfc) &= USED_MASK;
			FOOT_NOTE(pfc) &= USED_MASK;
			return pfc;
		}
		else
		{
			register void * pr;
			pr = nmAllocHeap(ph, size);
			if (NULL != pr)
			{
				memcpy(pr, pfc, sizeof(HEAD_NOTE(pfc)));
				_nmPutChunk(ph, pfc);
			}
			return pr;
		}
	}
}

