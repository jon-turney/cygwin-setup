#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"

#include <iostream>

static BoolOption testoption (false, 't', "testoption", "Tests the use of boolean options");
int
main (int argc, char **argv)
{
    std::vector <Option *> const &options (GetOption::GetInstance().optionsInSet());
    if (options.size() != 1) {
	std::cout << "Incorrect number of options in optionset" << std::endl;
	return 1;
    }
    if (options[0] != &testoption) {
	std::cout << "Incorrect option in default OptionSet" << std::endl;
	return 1;
    }
    return 0;
}
