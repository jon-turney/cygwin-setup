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
  T *getbykey (const U);
  /* add an element if it does not exist and return a reference */
  T & registerbykey (U);
  /* add an element if one with the same key does not exist */
  T & registerbyobject (T &);
  /* remove by index */
  T *removebyindex (size_t);
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
private:
  // ensure there is enough array space.
  void checksize (void);
  // perform an insertion.
  void insert (size_t, T *);
};

template < class T, class U, int
Ucmp (U, U) >
T * list < T, U, Ucmp >::getbykey (const U key)
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
      tempT = new T (key);
      size_t n;
      for (n = 0;
	   n < _number && Ucmp (pointerblock[n]->key, tempT->key) < 0; n++);
      insert (n, tempT);
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
      tempT = &newobj;
      size_t n;
      for (n = 0;
	   n < _number && Ucmp (pointerblock[n]->key, tempT->key) < 0; n++);
      insert (n, tempT);
    }
  return *tempT;
}

template < class T, class U, int
Ucmp (U, U) >
T * list < T, U, Ucmp >::removebyindex (size_t index)
{
  if (index && index <= _number)
    {
      T *rv = pointerblock[index - 1];
      /* remove from index - 1 */
      for (size_t i = index; i < _number; ++i)
	pointerblock[i - 1] = pointerblock[i];
      --_number;
      return rv;
    }
  return 0;
}

template < class T, class U, int
     Ucmp (U, U) > void
     list < T, U, Ucmp >::checksize ()
{
  if (_number == _space)
    {
      T **newptr = new (T *)[_space + 20];
      if (!newptr)
	{
	  //die - todo enable exceptions
	  exit (100);
	}
      for (size_t i = 0; i < _number; ++i)
	newptr[i] = pointerblock[i];
      delete[]pointerblock;
      pointerblock = newptr;
      /* TODO: Parameterise this too */
      _space += 20;
    }
}
template < class T, class U, int
     Ucmp (U, U) > void
     list < T, U, Ucmp >::insert (size_t where, T * someT)
{
  // doesn't change where
  checksize ();
  /* insert at where */
  for (size_t i = _number; where < i; --i)
    pointerblock[i] = pointerblock[i - 1];
  pointerblock[where] = someT;
  ++_number;
}


#endif /* _LIST_H_ */
