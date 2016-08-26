#ifndef SETUP_ROOT_H
#define SETUP_ROOT_H

#include "proppage.h"

class RootPage:public PropertyPage
{
public:
  RootPage ();
  virtual ~ RootPage ()
  {
  };

  bool Create ();

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code);
  virtual void OnInit ();
  virtual bool wantsActivation() const;
  virtual void OnActivate ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual long OnUnattended ();

 private:
  void check_if_enable_next (HWND h);
};

#endif /* SETUP_ROOT_H */
