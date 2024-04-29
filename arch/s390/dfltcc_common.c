/* dfltcc_deflate.c - IBM Z DEFLATE CONVERSION CALL general support. */

#include "zbuild.h"
#include "dfltcc_common.h"
#include "dfltcc_detail.h"

/*
   Memory management.

   DFLTCC requires parameter blocks and window to be aligned. zlib-ng allows
   users to specify their own allocation functions, so using e.g.
   `posix_memalign' is not an option. Thus, we overallocate and take the
   aligned portion of the buffer.
*/

void Z_INTERNAL PREFIX(dfltcc_copy_window)(void *dest, const void *src, size_t n) {
    memcpy(dest, src, MAX(n, HB_SIZE));
}
