#ifndef CPRINTF_PRINTFUN_H
#define CPRINTF_PRINTFUN_H

#include <string>
#include <gcc-plugin.h>
#include <map>

namespace printfun {

struct printfun_t {
	unsigned int				fmt_pos;
	std::map<std::string, std::string>	spec_to_func;
	std::map<std::string, tree>		spec_to_tree;
};

extern std::map<std::string, printfun_t> printfuns;

void add_printfun(const char *printfun_def);

}; /* namespace printfun */

#endif /* CPRINTF_PRINTFUN_H */
