#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "setup.h"
 void
lowmem ()
{
  fputs ("Insufficient memory.\n", stderr);
  exit (1);
}
void
winerror ()
{
  LPVOID msgbuf;
  int lasterr = GetLastError ();

    /* If !lasterr then assume a standard file error happened and was
       already displayed. */
    if (lasterr)

    {
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER
		      |FORMAT_MESSAGE_FROM_SYSTEM
		      |FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lasterr,
		      MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR) & msgbuf, 0, NULL);
      if (msgbuf)

	{
	  fprintf (stderr, "%s\n", msgbuf);
	  LocalFree (msgbuf);
	}

      else
	fprintf (stderr, "Unexpected error #%d\n", lasterr);
    }
}


