#include <stdexcept>

#include "log.h"
#include "printfun.h"

namespace printfun {

std::map<std::string, printfun_t> printfuns;

static const char *parse_get_fmt_pos(const char *printfun_def,
		unsigned int *out, std::string &func)
{
	size_t fmt_pos_len;

	if (*printfun_def != '(') {
		std::string err("Fmt-string argument position is unknown in function `");
		throw std::logic_error(err + func + "'");
	}
	printfun_def++;
	try {
		*out = std::stoul(printfun_def, &fmt_pos_len, 10);
	} catch(...) {
		std::string err("Invalid format position in `");
		throw std::logic_error(err + func + "' function");
	}
	if (fmt_pos_len == 0) {
		std::string err("Failed to parse format position of `");
		throw std::logic_error(err + func + "' function");
	}
	printfun_def += fmt_pos_len;
	if (*printfun_def != ')') {
		std::string err("Can't find closing brace for fmt-string position of `");
		throw std::logic_error(err + func + "' function");
	}
	printfun_def++;
	if (*printfun_def == '\0') {
		std::string err("Unexpected line end after `");
		throw std::logic_error(err + func + "' function");
	}
	return printfun_def;
}

static const char *parse_get_function(const char *printfun_def,
		std::string *out)
{
	while (ISBLANK(*printfun_def)) printfun_def++;
	if (*printfun_def == '\0')
		throw std::logic_error("No function name specified");
	if (!ISALPHA(*printfun_def) && *printfun_def != '_') {
		std::string err("Function name should start with character or underscore, not with: `");
		err += *printfun_def++;
		while (ISALPHA(*printfun_def) ||
				ISDIGIT(*printfun_def) ||
				*printfun_def == '_')
			err += *printfun_def++;
		throw std::logic_error(err + "'");
	}

	while (ISALPHA(*printfun_def) || ISDIGIT(*printfun_def) ||
			*printfun_def == '_')
		*out += *printfun_def++;
	return printfun_def;
}

static const char *parse_get_specifier(const char *printfun_def,
		std::string *out)
{
	while (ISBLANK(*printfun_def)) printfun_def++;
	if (*printfun_def == '\0')
		return printfun_def;
	if (*printfun_def != '%') {
		std::string err("Expected %-specifier, but got: `");
		while (*printfun_def != '\0' && !ISBLANK(*printfun_def))
			err += *printfun_def++;
		throw std::logic_error(err + "'");
	}
	printfun_def++;
	if (ISBLANK(*printfun_def))
		throw std::logic_error("Got empty %-specifier");
	while (!ISBLANK(*printfun_def)) {
		if (*printfun_def == '\0') {
			std::string err("Unexpected %-specifier end, got: `");
			err += "%";
			throw std::logic_error(err + *out + "'");
		}
		*out += *printfun_def++;
	}
	return printfun_def;
}

void add_printfun(const char *printfun_def)
{
	printfun_t pf;
	std::string fun_name;
	unsigned int i;

	printfun_def = parse_get_function(printfun_def, &fun_name);
	printfun_def = parse_get_fmt_pos(printfun_def,
			&pf.fmt_pos, fun_name);
	printfun_def++; /* skip function delimiter */

	for (i = 0;;i++) {
		std::string spec, func;

		printfun_def = parse_get_specifier(printfun_def, &spec);
		if (*printfun_def == '\0')
			break;
		printfun_def = parse_get_function(printfun_def, &func);
		printfun_def++; /* skip function delimiter */

		if (pf.spec_to_func.find(spec) != pf.spec_to_func.end()) {
			std::string err("%-Specifier `");
			err += spec;
			throw std::logic_error(err + "' found twice");
		}
		pf.spec_to_func[spec] = func;
		if (spec[0] == '%') { /* it's %% specifier really */
			if (spec.length() > 1) {
				std::string err("Found `%");
				err += spec;
				err += "' specifier for `";
				err += func;
				err += "', can handle only `%%'";
				throw std::logic_error(err);
			}
			log::info << "Reserved %% specifier for `"
				<< func << "'\n";
		}
	}

	if (i == 0) {
		std::string err("Found no %-specifiers for `");
		err += fun_name;
		throw std::logic_error(err + "' function");
	}

	if (printfuns.find(fun_name) != printfuns.end()) {
		std::string err("Function `");
		err += fun_name;
		throw std::logic_error(err + "' defined twice");
	}

	printfuns[fun_name] = pf;

	log::info << "Specifier handlers for `"
		<< fun_name << "(" << pf.fmt_pos << ")':\n";
	std::map<std::string, std::string>::const_iterator s;
	for (s = pf.spec_to_func.cbegin(); s != pf.spec_to_func.cend(); ++s)
		log::debug << "\t%" << (*s).first
			<< "\t" << (*s).second << std::endl;
}

}; /* namespace printfun */

