/* strarry.c: implementation of the strarry struct routines. */
#include <malloc.h>
#include "setup.h"
#include "strarry.h"

void
sa_init (SA * array)
{
  array->count = 0;
  array->array = NULL;
}

void
sa_add (SA * array, const char *str)
{
  array->array = array->count
    ? xrealloc (array->array, sizeof (char *) * (array->count + 1))
  : xmalloc (sizeof (char *));
  array->array[array->count++] = xstrdup (str);
}

void
sa_cleanup (SA * array)
{
  size_t n = array->count;
  while (n--)
    xfree (array->array[n]);
  xfree (array->array);
}
