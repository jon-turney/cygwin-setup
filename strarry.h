/* strarry.h: strarry struct */

#include <stddef.h>
typedef struct strarry
{
  char **array;
  size_t count;
  size_t index;
} SA;
void sa_init (SA *);		/* Initialize the struct. */
void sa_add (SA *, const char *);	/* Add a string to the array. */
void sa_cleanup (SA *);	/* Deallocate all of the memory. */

