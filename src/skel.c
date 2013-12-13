/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#define WEAK __attribute__ ((weak))

#include "LPC17xx.h"

#include "mri.h"

// int _write(int, const char *, int);

void WEAK _exit(int i) {
	while (1)
		NVIC_SystemReset();
}

int WEAK _kill(int pid, int signal) {
	return 0;
}

int WEAK _getpid() {
	return 0;
}

int WEAK __aeabi_atexit(void *object, void (*destructor)(void *), void *dso_handle) {
	return 0;
}

int WEAK _write(int fd, void *buf, size_t buflen) {
// 		_dbg_init();
// 		dbg->send((uint8_t *) buf, buflen);
	return buflen;
}

int WEAK _close(int fd) {
	return 0;
}

int WEAK _lseek(int file, int ptr, int dir) {
	return ptr;
}

int WEAK _read(int file, char *buf, int len) {
// 		_dbg_init();
// 		return dbg->recv((uint8_t *) buf, len);
	return 0;
}

void* WEAK _sbrk(int incr) {

	extern char __bss_end__; // Defined by the linker
	static char *heap_end = &__bss_end__;
	char *prev_heap_end = heap_end;

	char * stack = (char*) __get_MSP();
	if (heap_end + incr >  stack)
	{
		_write (0, "Heap and stack collision\n", 25);
		__debugbreak();
		//errno = ENOMEM;
// 		return  (void *) -1;
		//abort ();
	}

	heap_end += incr;
	return prev_heap_end;
}

int WEAK _fstat(int file, void *st) {
	return 0;
}

int WEAK _isatty(int fd) {
	if (fd <= 2)
		return 1;
	return 0;
}

void WEAK __aeabi_unwind_cpp_pr0(void){}
// void __libc_init_array(void){}
// int __atexit(void(*f)(void)){ return 0; }
int atexit(void(*f)(void)){ return 0; }

/******************************************************************************
 *                                                                            *
 * Small malloc and free implementation                                       *
 *                                                                            *
 ******************************************************************************/

#ifndef MDEBUG
#define MDEBUG(...) do {} while (0)
// #define MDEBUG(...) printf(__VA_ARGS__)
#endif

typedef struct __attribute__ ((packed))
{
	uint32_t last :1;
	uint32_t used :1;
	uint32_t next :30;
	
	uint8_t  data[];
} _alloc_block;

static _alloc_block* _malloc_start = NULL;
static _alloc_block* _malloc_end   = NULL;
extern void* __end__; // from linker
extern void* __HeapLimit; // from linker
#define offset(x) (((uint8_t*) x) - ((uint8_t*) _malloc_start))
#define _malloc_size(walk) ((uintptr_t) (((uint8_t*) ((uintptr_t)walk->next)) - ((uint8_t*) walk)))
void* malloc(size_t nbytes)
{
	// nbytes = ceil(nbytes / 4) * 4
	if (nbytes & 3)
		nbytes += 4 - (nbytes & 3);
	
	// start at the start
	_alloc_block* p = ((_alloc_block*) _malloc_start);
	
	if (p == NULL)
	{
		p = _malloc_start = _sbrk(sizeof(_alloc_block) + nbytes);
		p->next = sizeof(_alloc_block) + nbytes;
		p->used = 0;
		p->last = 1;
		
		_malloc_end = (_alloc_block*) (((uint8_t*) p) + sizeof(_alloc_block) + nbytes);
	}
	
	// find the allocation size including our metadata
	uint16_t nsize = nbytes + sizeof(_alloc_block);
	
	MDEBUG("\tallocate %d bytes from heap at %p\n", nsize, _malloc_start);
	
	// now we walk the list, looking for a sufficiently large free block
	do {
		MDEBUG("\t\tchecking %p (%s, %db%s)\n", p, (p->used?"used":"free"), p->next, (p->last?", last":""));
		if ((p->used == 0) && (p->next >= nsize))
		{   // we found a free space that's big enough
			MDEBUG("\t\tFOUND free block at %p (+%d) with %d bytes\n", p, offset(p), p->next);
			// mark it as used
			p->used = 1;
			
			// if there's free space at the end of this block
			if (p->next > nsize)
			{
				// q = p->next
				_alloc_block* q = (_alloc_block*) (((uint8_t*) p) + nsize);
				
				MDEBUG("\t\twriting header to %p (+%d) (%d)\n", q, offset(q), p->next - nsize);
				// write a new block header into q
				q->used = 0;
				q->next = p->next - nsize;
				q->last = p->last;
				p->last = 0;
				
				// set our next to point to it
				p->next = nsize;
				
// 				// move sbrk so we know where the end of the list is
// 				if (offset(q) > sbrk)
// 					sbrk = offset(q);
// 				
// 				// sanity check
// 				if (sbrk > size)
// 				{
// 					// captain, we have a problem!
// 					// this can only happen if something has corrupted our heap, since we should simply fail to find a free block if it's full
// 					__debugbreak();
// 				}
			}
// 			
// 			MDEBUG("\t\tsbrk is %d (%p)\n", sbrk, ((uint8_t*) base) + sbrk);
			
			// then return the data region for the block
			return &p->data;
		}
		
		if (p->last)
			break;
		
		// p = p->next
		p = (_alloc_block*) (((uint8_t*) p) + p->next);
		
		// make sure we don't walk off the end
	} while (p <= _malloc_end);
	
	// get a new block from sbrk
	_alloc_block* q = (_alloc_block*) _sbrk(nsize);
	
	if (q)
	{
		MDEBUG("\t\tsbrk gave new  %db block at %p\n", nsize, q);
		
		p->next = ((uint8_t*) q) - ((uint8_t*) p);
		q->next = nsize;
		q->used = 1;
		p->last = 0;
		q->last = 1;
		
		return q->data;
	}
	
	MDEBUG("\t\tsbrk returned null! size: %d, last block at %p\n", nsize, p);
	
	// fell off the end of the region!
	return NULL;
}

void free(void* alloc)
{
// 	_alloc_block* p = (_alloc_block*) (((uint8_t*) alloc) - sizeof(_alloc_block));
	_alloc_block* p = ((_alloc_block*) alloc) - 1;
	p->used = 0;
	
	MDEBUG("\tdeallocating %p (+%d, %db)\n", p, offset(p), p->next);
	
	// combine next block if it's free
	_alloc_block* q = (_alloc_block*) (((uint8_t*) p) + p->next);
	MDEBUG("\t\tchecking %p (%s, %db%s)\n", q, (q->used?"used":"free"), q->next, (q->last?", last":""));
	if (q->used == 0)
	{
		MDEBUG("\t\tCombining with next free region at %p, new size is %d\n", q, p->next + q->next);
		
// 		// if q was the last block, move sbrk back to p (the deallocated block)
// 		if (offset(q) >= sbrk)
// 			sbrk = offset(p);
// 		
// 		MDEBUG("\t\tsbrk is %d (%p)\n", sbrk, ((uint8_t*) base) + sbrk);
// 		
// 		// sanity check
// 		if (sbrk > size)
// 		{
// 			// captain, we have a problem!
// 			// this can only happen if something has corrupted our heap, since we should simply fail to find a free block if it's full
// 			__debugbreak();
// 		}
		
		p->next += q->next;
		p->last =  q->last;
	}
	
	// walk the list to find previous block
	q = (_alloc_block*) _malloc_start;
	do {
		MDEBUG("\t\tchecking %p (%s, %db%s)\n", q, (q->used?"used":"free"), q->next, (q->last?", last":""));
		// check if q is the previous block
		if ((((uint8_t*) q) + q->next) == (uint8_t*) p) {
			// q is the previous block.
			if (q->used == 0)
			{ // if q is free
				MDEBUG("\t\tCombining with previous free region at %p, new size is %d\n", q, p->next + q->next);
				
				// combine!
				q->next += p->next;
				q->last =  p->last;
				
// 				// if this block was the last one, then set sbrk back to the start of the previous block we just combined
// 				if ((offset(p) + p->next) >= sbrk)
// 					sbrk = offset(q);
// 				
// 				MDEBUG("\t\tsbrk is %d (%p)\n", sbrk, ((uint8_t*) base) + sbrk);
// 				
// 				// sanity check
// 				if (sbrk > size)
// 				{
// 					// captain, we have a problem!
// 					// this can only happen if something has corrupted our heap, since we should simply fail to find a free block if it's full
// 					__debugbreak();
// 				}
			}
			
			// we found previous block, return
			return;
		}
		
		// return if last block
		if (q->last)
			return;
		
		// q = q->next
		q = (_alloc_block*) (((uint8_t*) q) + q->next);
		
		// if some idiot deallocates our memory region while we're using it, strange things can happen.
		// avoid an infinite loop in that case, however we'll still leak memory and may corrupt things
		if (q->next == 0)
			return;
		
		if (q->last)
			return;
		
		// make sure we don't walk off the end
	} while (q < (_alloc_block*) (_malloc_end));
}
