#include <malloc.h>
#include <string.h>
#include <windows.h>
#include "setup.h"

void *
xmalloc (size_t size)
{
  void *mem = malloc (size);

  if (!mem)
    lowmem ();
  return mem;
}

void *
xrealloc (void *orig, size_t newsize)
{
  void *mem = realloc (orig, newsize);

  if (!mem)
    lowmem ();
  return mem;
}

char *
xstrdup (const char *arg)
{
  char *str = strdup (arg);

  if (!str)
    lowmem ();
  return str;
}
