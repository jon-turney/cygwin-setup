#include "GetOption.h"
#include "BoolOption.h"

static BoolOption testoption (false, 't', "testoption", "Tests the use of boolean options");
int
main (int argc, char **argv)
{
  if (!GetOption::GetInstance().Process (argc, argv))
    {
      cout << "Failed to process options" << endl;
      return 1;
    }
  if (testoption)
    {
      cout << "Option used" << endl;
      return 1;
    }
  else
    {
      cout << "Option not used" << endl;
      return 0;
    }
}
