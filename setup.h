#include <stddef.h>

/* Routines in error.c. */
void lowmem ();			/* Report low memory and exit the
				   application. */
void winerror ();		/* Report errors from GetLastError. */

/* Routines in memory.c. */
void *xmalloc (size_t);		/* Like malloc, but exit on error. */
void *xrealloc (void *, size_t);	/* Like realloc, but exit on error. */
char *xstrdup (const char *);	/* Like strdup, but exit on error. */

#define xfree(ptr) free(ptr)
