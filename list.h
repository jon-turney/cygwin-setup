/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#ifndef _LIST_H_
#define _LIST_H_

#include "stdlib.h"
#include "sys/types.h"

/* Creates a list/array of type T (must have a field key of type U),
 * with index type U and U comparison Ucmp. 
 */
template < class T, class U, int
Ucmp (U, U) >
class list
{
  T **pointerblock;
  size_t _number;
  size_t _space;

public:
list ():pointerblock (0), _number (0), _space (0)
  {
  };
  // ~list () ...
  /* get an element if it exists */
  T *getbykey (U);
  /* add an element if it does not exist and return a reference */
  T & registerbykey (U);
  /* add an element if one with the same key does not exist */
  T & registerbyobject (T &);
  /* remove by index */
  T * removebyindex (size_t);
  size_t number () const
  {
    return _number;
  };
  /* get by offset - not thread safe - starts at 1 */
  T *operator [] (size_t n) const
  {
    return n && n <= _number ? pointerblock[n - 1] : 0;
  };
  /* TODO: delete */
};

template < class T, class U, int
Ucmp (U, U) >
T * list < T, U, Ucmp >::getbykey (U key)
{
  for (size_t n = 0; n < _number; n++)
    if (Ucmp (pointerblock[n]->key, key) == 0)
      return pointerblock[n];
  return 0;
}


template < class T, class U, int
Ucmp (U, U) >
T & list < T, U, Ucmp >::registerbykey (U key)
{
  T *tempT = getbykey (key);
  if (!tempT)
    {
      if (_number == _space)
	{
	  T **newptr =
	    (T **) realloc (pointerblock, sizeof (T *) * (_space + 20));
	  if (!newptr)
	    {
	      //die - todo enable exceptions
	      exit (100);
	    }
	  pointerblock = newptr;
	  /* TODO: Parameterise this too */
	  _space += 20;
	}
      tempT = new T (key);
      size_t n;
      for (n = 0;
	   n < _number && Ucmp (pointerblock[n]->key, tempT->key) < 0; n++);
      /* insert at n */
      memmove (&pointerblock[n + 1], &pointerblock[n],
	       (_number - n) * sizeof (T *));
      pointerblock[n] = tempT;
      _number++;
    }
  return *tempT;
}

/* TODO: factor both insertion routines into one */
template < class T, class U, int
Ucmp (U, U) >
T & list < T, U, Ucmp >::registerbyobject (T & newobj)
{
  T *tempT = getbykey (newobj.key);
  if (!tempT)
    {
      if (_number == _space)
	{
	  T **newptr =
	    (T **) realloc (pointerblock, sizeof (T *) * (_space + 20));
	  if (!newptr)
	    {
	      //die - todo enable exceptions
	      exit (100);
	    }
	  pointerblock = newptr;
	  /* TODO: Parameterise this too */
	  _space += 20;
	}
      tempT = &newobj;
      size_t n;
      for (n = 0;
	   n < _number && Ucmp (pointerblock[n]->key, tempT->key) < 0; n++);
      /* insert at n */
      memmove (&pointerblock[n + 1], &pointerblock[n],
	       (_number - n) * sizeof (T *));
      pointerblock[n] = tempT;
      _number++;
    }
  return *tempT;
}

template < class T, class U, int Ucmp (U, U) >
T * list < T, U, Ucmp >::removebyindex (size_t index)
{
  if (index && index <= _number)
    {
      T * rv = pointerblock[index - 1];
      /* remove from index - 1 */
      memmove (&pointerblock[index - 1], &pointerblock[index], (_number - index) * sizeof (T *));
      _number --;
      return rv;
    }
  return 0;
}

#endif /* _LIST_H_ */
