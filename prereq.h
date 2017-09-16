#ifndef SETUP_PREREQ_H
#define SETUP_PREREQ_H

#include <map>
#include "proppage.h"
#include "PackageTrust.h"
#include "package_meta.h"

using namespace std;

// keeps the map sorted by name
struct packagemeta_ltcomp
{
  bool operator() ( const packagemeta *m1, const packagemeta *m2 )
    { return casecompare(m1->name, m2->name) < 0; }
};


class PrereqPage:public PropertyPage
{
public:
  PrereqPage ();
  virtual ~PrereqPage () { };
  bool Create ();
  virtual void OnInit ();
  virtual void OnActivate ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual long OnUnattended ();
private:
  long whatNext ();
};

class PrereqChecker
{
public:
  // returns true if no dependency problems exist
  bool isMet ();

  // formats 'unmet' as a string for display
  void getUnmetString (std::string &s);

  static void setUpgrade (bool u) { upgrade = u; };
  static void setTestPackages (bool t) { use_test_packages = t; };

private:
  static bool upgrade;
  static bool use_test_packages;
};

#endif /* SETUP_PREREQ_H */
