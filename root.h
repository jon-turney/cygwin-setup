#ifndef CINSTALL_ROOT_H
#define CINSTALL_ROOT_H

#include "proppage.h"

class RootPage:public PropertyPage
{
public:
  RootPage ()
  {
  };
  virtual ~ RootPage ()
  {
  };

  bool Create ();

  virtual void OnInit ();
  virtual long OnNext ();
  virtual long OnBack ();
};

#endif // CINSTALL_ROOT_H
