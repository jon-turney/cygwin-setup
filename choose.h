/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef _CHOOSE_H_
#define _CHOOSE_H_

#include "proppage.h"

class Category;
class packagemeta;

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1

struct _header
{
  const char *text;
  int slen;
  int width;
  int x;
};

#ifdef __cplusplus
#include "list.h"
#include "package_meta.h"

class view;

class pick_line
{
public:
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat) = 0;
  virtual int click (int const myrow, int const ClickedRow, int const x) = 0;
  virtual int itemcount () const = 0;
  // this may indicate bad inheritance model.
  virtual bool IsContainer (void) const = 0;
  virtual void insert (pick_line &) = 0;
  // Never allocate to key, always allocated elsewhere
  char const *key;
    virtual ~ pick_line ()
  {
  };
protected:
  pick_line ()  {  };
  pick_line (char const *aKey):key(aKey) {};
  pick_line (pick_line const &);
  pick_line & operator= (pick_line const &);
};

class pick_pkg_line:public pick_line
{
public:
  pick_pkg_line (packagemeta & apkg):pkg (apkg)
  {
    key = apkg.key;
  };
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat);
  virtual int click (int const myrow, int const ClickedRow, int const x);
  virtual int itemcount () const
  {
    return 1;
  }
  virtual bool IsContainer (void) const
  {
    return false;
  }
  virtual void insert (pick_line &)
  {
  };
private:
  packagemeta & pkg;
};

class pick_category_line:public pick_line
{
public:
  pick_category_line (Category & _cat, size_t thedepth = 0, bool aBool =
		      true, bool aBool2 = true):pick_line (_cat.key), current_default (Default_action), 
  cat (_cat), labellength (0), depth(thedepth)
  {
    if (aBool)
      {
        collapsed = true;
	show_label = true;
      }
    else
      {
	collapsed = false;
	show_label = aBool2;
      }
  };
  ~pick_category_line (){ empty (); }
  void ShowLabel(bool aBool = true) {show_label = aBool; if (!show_label) collapsed = false;}
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat);
  virtual int click (int const myrow, int const ClickedRow, int const x);
  virtual int itemcount () const
  {
    if (collapsed)
      return 1;
    int t = show_label ? 1 : 0;
    for (size_t n = 1; n <= bucket.number (); n++)
        t += bucket[n]->itemcount ();
      return t;
  };
  virtual bool IsContainer (void) const
  {
    return true;
  }
  virtual void insert (pick_line & aLine)
  {
    bucket.registerbyobject (aLine);
  }
  void empty ();
private:
  enum _actions {
    Default_action,
    Install_action,
    Reinstall_action,
    Uninstall_action
  } current_default;
  char const * actiontext();
  Category & cat;
  bool collapsed;
  bool show_label;
  size_t labellength;
  size_t depth;
  pick_category_line (pick_category_line const &);
  pick_category_line & operator= (pick_category_line const &);
  list < pick_line, char const *, strcasecmp > bucket;
};

class view
{
  class views;
public:
  int num_columns;
  views get_view_mode () 
  {
    return view_mode;
  };
  void set_view_mode (views _mode);
  struct _header *headers;
  view (views mode, HWND listview, Category &cat);
  const char *mode_caption ();
  void insert_pkg (packagemeta &);
  void insert_category (Category *, bool);
  void clear_view (void);
  int click (int row, int x);
  int current_col;
  int new_col;
  int src_col;
  int cat_col;
  int pkg_col;
  int last_col;
  pick_category_line contents;
  void scroll (HWND hwnd, int which, int *var, int code);
  HWND ListHeader (void) const
  {
    return listheader;
  }


  class views {
    public:
    static const views Unknown;
    static const views PackageFull;
    static const views Package;
    static const views Category;
    static const views NView;
    views () : _value (0) {};
    views (int aInt) { _value = aInt; if (_value < 0 || _value > 3)
      _value = 0;}
    views& operator++ ();
    bool operator == (views const &rhs) {return _value == rhs._value;}
    bool operator != (views const &rhs) {return _value != rhs._value;}
    const char * caption ();
    
    private:
     int _value;
  };
  
private:
    HWND listview;
  HWND listheader;
  views view_mode;
  void set_headers ();
  void init_headers (HDC dc);

};

class ChooserPage:public PropertyPage
{
public:
  ChooserPage ()
  {
  };
  virtual ~ ChooserPage ()
  {
  };

  virtual bool OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam);

  bool Create ();
  virtual void OnActivate ();
};

#endif /* __cplusplus */
#endif /* _CHOOSE_H_ */
