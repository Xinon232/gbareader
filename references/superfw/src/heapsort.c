/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ronnie Kon at Mindcraft Inc., Kevin Lew and Elmer Yglesias.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// Adapted for GBA by David Guillen Fandos
//
// Elements must be multiple of 4 bytes, and aligned. Consequently it only
// performs aligned accesses (so it is suitable to use in 16 bit buses).

#include <stdint.h>
#include <alloca.h>

#include "gbahw.h"

// uint32_t buffer swapping.
static inline void SWAP4(uint32_t *a, uint32_t *b, unsigned count) {
	for (unsigned c = 0; c < count; c++) {
		uint32_t tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}

// Copy words from one buffer to another
static inline void COPY4(uint32_t *dst, const uint32_t *src, unsigned count) {
	for (unsigned c = 0; c < count; c++)
		*dst++ = *src++;
}


/*
 * Select the top of the heap and 'heapify'.  Since by far the most expensive
 * action is the call to the compar function, a considerable optimization
 * in the average case can be achieved due to the fact that k, the displaced
 * elememt, is ususally quite small, so it would be preferable to first
 * heapify, always maintaining the invariant that the larger child is copied
 * over its parent's record.
 *
 * Then, starting from the *bottom* of the heap, finding k's correct place,
 * again maintianing the invariant.  As a result of the invariant no element
 * is 'lost' when k is assigned its correct place in the heap.
 *
 * The time savings from this optimization are on the order of 15-20% for the
 * average case. See Knuth, Vol. 3, page 158, problem 18.
 *
 */

/*
 * Heapsort -- Knuth, Vol. 3, page 145.  Runs in O (N lg N), both average
 * and worst.  While heapsort is faster than the worst case of quicksort,
 * the BSD quicksort does median selection so that the chance of finding
 * a data set that will trigger the worst case is nonexistent.  Heapsort's
 * only advantage over quicksort is that it requires little additional memory.
 */
void heapsort4(
  void *vbase, unsigned nmemb, unsigned size,
  int (*compar)(const void *, const void *)
) {
	if (nmemb <= 1 || !size)
		return;
	// Alloca should be at least 4 byte aligned
	uint32_t tmpbuf[size];

	/*
	 * Items are numbered from 1 to nmemb, so offset from size bytes
	 * below the starting address.
	 */
	uint32_t *base = (uint32_t*)vbase;
	base -= size;

	// Convert the array into a heap by enforcing the heap invariant.
	for (unsigned l = nmemb / 2; l > 0; l--) {
		unsigned par = l, child0 = par * 2;
		while (child0 <= nmemb) {
			uint32_t *parentptr = &base[size * par];
			uint32_t *childptr0 = &base[size * child0];
			uint32_t *childptr1 = &base[size * (child0 + 1)];
			// Pick the biggest children out of the two.
			// * There two cases.  If j == nmemb, select largest of Ki and Kj.
			// * If j < nmemb, select largest of Ki, Kj and Kj+1.
			par = (child0 < nmemb && compar(childptr0, childptr1) < 0) ? child0 + 1 : child0;
			uint32_t *bgchptr = &base[size * par];
			if (compar(bgchptr, parentptr) <= 0)
				break;
			SWAP4(parentptr, bgchptr, size);
			child0 = par * 2;
		}
	}

	// Moves the largest element (the top one) to its final position, which
	// is at the end of the heap/array. Then proceed to re-heapify.
	// To avoid SWAPs, use a temporary buffer.
	while (nmemb > 1) {
		// Copy last element into tmp buf, overwrite it with the first element.
		COPY4(tmpbuf, &base[nmemb * size], size);
		COPY4(&base[nmemb * size], &base[size], size);
		--nmemb;   // The heap got smaller by one element.

		// Like heapify, but the parent element can be ignored (was taken out)
		// We just copy and overwrite the items (imagine it is "empty")
		unsigned par = 1, child0 = par * 2;
		while (child0 <= nmemb) {
			uint32_t *parentptr = &base[size * par];
			uint32_t *childptr0 = &base[size * child0];
			uint32_t *childptr1 = &base[size * (child0 + 1)];
			// Pick the biggest children out of the two.
			par = (child0 < nmemb && compar(childptr0, childptr1) < 0) ? child0 + 1 : child0;
			uint32_t *bgchptr = &base[size * par];
			COPY4(parentptr, bgchptr, size);
			child0 = par * 2;
		}
		// Find the right place for K. It will usually be "par"
		// but we might need to move some elements down before reaching it.
		while (1) {
			child0 = par;
			par = child0 / 2;
			uint32_t *childptr0 = &base[size * child0];
			uint32_t *parentptr = &base[size * par];
			if (child0 == 1 || compar(tmpbuf, parentptr) < 0) {
				COPY4(childptr0, tmpbuf, size);
				break;
			}
			COPY4(childptr0, parentptr, size);
		}
	}
}

