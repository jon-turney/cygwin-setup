#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"

#include <iostream>

static BoolOption testoption (false, 't', "testoption", "Tests the use of boolean options");
int
main (int argc, char **argv)
{
  if (!GetOption::GetInstance().Process (argc, argv))
    {
      std::cout << "Failed to process options" << std::endl;
      return 1;
    }
  if (testoption)
    {
      std::cout << "Option used" << std::endl;
      return 1;
    }
  else
    {
      std::cout << "Option not used" << std::endl;
      return 0;
    }
}
