/**
 * \file nds-slot2-virt.c
 */

#ifndef __3DS__

#include <nds.h>
/* For struct mallinfo, which dlmalloc.h leaves to it */
#include <malloc.h>
#include "../z-virt.h"
#include "../z-util.h"
#include "dlmalloc.h"
#include "nds-slot2-ram.h"

static uint32_t mem_alt_ptr_mask, mem_alt_ptr_value;
static mspace mem_alt_mspace;

void mem_init_alt(void) {
#ifdef BLOCKSDS
	if (peripheralSlot2Init(SLOT2_PERIPHERAL_EXTRAM) && peripheralSlot2RamStart()) {
		peripheralSlot2Open(SLOT2_PERIPHERAL_EXTRAM);
		peripheralSlot2EnableCache(true);
		mem_alt_ptr_mask = 0xF8000000;
		mem_alt_ptr_value = 0x08000000;

		mem_alt_mspace = create_mspace_with_base(peripheralSlot2RamStart(), peripheralSlot2RamSize(), 0);
#else
	if (ram_init(DETECT_RAM)) {
		mem_alt_ptr_mask = 0xFE000000;
		mem_alt_ptr_value = 0x08000000;

		mem_alt_mspace = create_mspace_with_base(ram_unlock(), ram_size(), 0);
#endif
	} else {
		mem_alt_ptr_mask = 0xFF000000;
		mem_alt_ptr_value = 0x06000000;

		/* Update if VRAM allocation in nds-draw.c is changed! */
		mem_alt_mspace = create_mspace_with_base((void *)0x06860000, 0x068A4000 - 0x06860000, 0);
	}
}

#define SZ(uptr)	*((size_t *)((char *)(uptr) - sizeof(size_t)))

void *mem_alloc_alt(size_t len)
{
	char *mem;

	/* Allow allocation of "zero bytes" */
	if (len == 0) return (NULL);
	if (len & 1) len++;

	mem = mspace_malloc(mem_alt_mspace, len + sizeof(size_t));
	if (!mem)
		return mem_alloc(len);

	mem += sizeof(size_t);
	SZ(mem) = len;

	return mem;
}

/*
 * Zeroed two bytes at a time rather than with memset(), which is free to
 * write single bytes at either end: see string_make_alt().  The length was
 * rounded up to even when the block was allocated.
 */
void *mem_zalloc_alt(size_t len)
{
	volatile uint16_t *mem = mem_alloc_alt(len);

	for (size_t i = 0; i < (len + 1) / 2; i++)
		mem[i] = 0;

	return (void *) mem;
}

void mem_free_alt(void *p)
{
	if (!p) return;

	if ((((u32) p) & mem_alt_ptr_mask) != mem_alt_ptr_value) {
		mem_free(p);
		return;
	}

	mspace_free(mem_alt_mspace, (char *)p - sizeof(size_t));
}

void *mem_realloc_alt(void *p, size_t len)
{
	char *m = p;

	/* Fail gracefully */
	if (len == 0) return (NULL);
	if (!m) return mem_realloc(p, len);
	if ((((u32) p) & mem_alt_ptr_mask) != mem_alt_ptr_value) return mem_realloc(p, len);

	if (len & 1) len++;

	m = mspace_realloc(mem_alt_mspace, m - sizeof(size_t), len + sizeof(size_t));
	if (!m) {
		m = malloc(len + sizeof(size_t));
		if (!m) quit("Out of Memory!");
		memcpy(m + sizeof(size_t), p, len);
		mspace_free(mem_alt_mspace, p);
	}

	m += sizeof(size_t);
	SZ(m) = len;

	return m;
}

/*
 * Take a block back out of the alternative heap, keeping len bytes of it, for
 * mem_realloc(): whoever grows a block means to write to it.
 */
void *mem_realloc_to_main(void *p, size_t len)
{
	void *m = mem_alloc(len);

	memcpy(m, p, MIN(len, SZ(p)));
	mem_free_alt(p);

	return m;
}

/*
 * A copy of a string in the alternative heap, written two bytes at a time:
 * the slot-2 bus, like VRAM, cannot write one byte on its own, and writes it
 * to both halves of the halfword instead.  Volatile, so that the compiler can
 * neither turn the loop into a memcpy() nor split its stores.  The copy is to
 * be read from and not written to; mem_realloc() takes it back out first.
 */
char *string_make_alt(const char *str)
{
	size_t len = strlen(str) + 1;
	volatile uint16_t *m = mem_alloc_alt(len);

	for (size_t i = 0; i < len; i += 2) {
		uint16_t hi = (i + 1 < len) ? (uint8_t) str[i + 1] : 0;

		m[i / 2] = (uint8_t) str[i] | (hi << 8);
	}

	return (char *) m;
}

size_t mem_free_space_alt(void) {
	return mspace_mallinfo(mem_alt_mspace).fordblks;
}

bool mem_is_alt_alloc(void *p) {
	return (((u32) p) & mem_alt_ptr_mask) == mem_alt_ptr_value;
}

#endif
