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

typedef struct __attribute__ ((packed))
{
	uintptr_t last :1;
	uintptr_t free :1;
	uintptr_t next :30;
	
	uint8_t  data[];
} _alloc_block;

static _alloc_block* _malloc_start = NULL;

#define _malloc_size(walk) ((uintptr_t) (((uint8_t*) ((uintptr_t)walk->next)) - ((uint8_t*) walk)))
void* malloc(size_t size)
{
	if (size & 3)
		size += (4 - (size & 3)); // round up to 4-aligned value

	if (_malloc_start == NULL)
	{
		_malloc_start = _sbrk(sizeof(_alloc_block) + size);
		_malloc_start->next = ((uintptr_t) _malloc_start) + sizeof(_alloc_block) + size;
		_malloc_start->last = 1;
		_malloc_start->free = 0;
		return _malloc_start->data;
	}

	_alloc_block* walk = _malloc_start;
	do
	{
		if (walk->free && (_malloc_size(walk) >= (size + sizeof(_alloc_block))))
		{
			walk->free = 0;
			if (_malloc_size(walk) > size)
			{
				_alloc_block* n = (_alloc_block*) (((uint8_t*) ((uintptr_t) walk)) + sizeof(_alloc_block) + size);
				n->next = walk->next;
				n->last = walk->last;
				n->free = 1;
				walk->last = 0;
				walk->next = ((uintptr_t) n);
			}
			return walk->data;
		}
		
		if (walk->last)
			break;

		walk = (_alloc_block*) ((uintptr_t) walk->next);
	}
	while ((walk->last == 0) && walk->next);

	walk->next = (uint32_t) _sbrk(size + sizeof(_alloc_block));

	if (walk->next == 0)
		return NULL;

	walk->last = 0;

	walk = (_alloc_block*) ((uintptr_t) walk->next);
	walk->next = 0;
	walk->free = 0;
	walk->last = 1;

	return walk->data;
}

void free(void* alloc)
{
	_alloc_block* freed = (_alloc_block*) (((uintptr_t) alloc) - 4);

	if (freed->free)
	{
		_write(0, "Double free!\n", 13);
		__debugbreak();
	}

	freed->free = 1;

	// combine with next free section
	if (freed->last == 0)
	{
		_alloc_block* next = (_alloc_block*) ((uintptr_t) freed->next);
		if (next->free)
		{
			freed->next = next->next;
			freed->last = next->last;
		}
	}

	// combine with previous free section
	_alloc_block* walk = _malloc_start;
	do
	{
		if (walk->next == ((uintptr_t) freed))
		{
			if (walk->free)
			{
				walk->next = freed->next;
				walk->last = freed->last;
			}
			break;
		}

		walk = (_alloc_block*) ((uintptr_t) walk->next);
	}
	while ((walk->last == 0) && walk->next);
}
