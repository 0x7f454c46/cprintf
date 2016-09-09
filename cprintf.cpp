#include <iostream>
#include <vector>
#include <map>

#include <gcc-plugin.h>
#include <plugin-version.h>

int plugin_is_GPL_compatible = 1;

namespace log {
	enum log_level_t {
		LOG_NONE,
		LOG_ERROR,
		LOG_WARN,
		LOG_INFO,
		LOG_DEBUG,
		LOG_ALL
	};

	static std::map<std::string, log_level_t> log_level_map_create(void)
	{
		std::map<std::string, log_level_t> ret;

		ret["quite"]	= LOG_NONE;
		ret["no"]	= LOG_NONE;
		ret["none"]	= LOG_NONE;
		ret["off"]	= LOG_NONE;
		ret["Quite"]	= LOG_NONE;
		ret["No"]	= LOG_NONE;
		ret["None"]	= LOG_NONE;
		ret["Off"]	= LOG_NONE;
		ret["error"]	= LOG_ERROR;
		ret["Error"]	= LOG_ERROR;
		ret["err"]	= LOG_ERROR;
		ret["Err"]	= LOG_ERROR;
		ret["warn"]	= LOG_WARN;
		ret["warning"]	= LOG_WARN;
		ret["Warn"]	= LOG_WARN;
		ret["Warning"]	= LOG_WARN;
		ret["info"]	= LOG_INFO;
		ret["Info"]	= LOG_INFO;
		ret["debug"]	= LOG_DEBUG;
		ret["Debug"]	= LOG_DEBUG;
		ret["all"]	= LOG_ALL;
		ret["All"]	= LOG_ALL;
		ret["ALL"]	= LOG_ALL;

		return ret;
	}
	const static std::map<std::string, log_level_t>
		log_level_map = log_level_map_create();

	class logger_t {
		log_level_t log_level = LOG_WARN;
	public:
		log_level_t curr_level(void) const { return log_level; }
		void set_level(const char *level)
		{
			try {
				log_level = log_level_map.at(level);
			} catch (const std::out_of_range &oor) {
				std::string err("No such log level: `");
				err += level;
				throw std::logic_error(err + "'");
			}
		}
	} logger;
	static void set_log_level(const char *level)
	{
		logger.set_level(level);
	}

	class log_stream {
		const log_level_t level;
	public:
		log_stream(log_level_t l):level(l) {}

		template <typename T>
		const log_stream& operator<<(const T& v) const
		{
			if (logger.curr_level() >= level)
				std::cerr << v;
			return *this;
		}

		typedef
		std::basic_ostream<char, std::char_traits<char>> std_stream;
		typedef std_stream& (*std_manip)(std_stream&);
		const log_stream& operator<<(std_manip manip) const
		{
			if (logger.curr_level() >= level)
				manip(std::cerr);
			return *this;
		}
	} err(LOG_ERROR), warn(LOG_WARN), info(LOG_INFO),
	      debug(LOG_DEBUG), all(LOG_ALL);
};

namespace printfun {
	struct printfun_t {
		unsigned int				fmt_pos;
		std::map<std::string, std::string>	spec_to_func;
	};
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
		if (!ISALPHA(*printfun_def)) {
			std::string err("Function name should start with character: `");
			err += *printfun_def++;
			while (ISALPHA(*printfun_def) || ISDIGIT(*printfun_def))
				err += *printfun_def++;
			throw std::logic_error(err + "'");
		}

		while (ISALPHA(*printfun_def) || ISDIGIT(*printfun_def))
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

	static void parse_printfun(const char *printfun_def)
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

		log::info << "Added new function to printfuns: `"
			<< fun_name << "(" << pf.fmt_pos << "):\n";
		std::map<std::string, std::string>::const_iterator s;
		for (s = pf.spec_to_func.cbegin(); s != pf.spec_to_func.cend(); ++s)
			log::debug << "\t%" << (*s).first
				<< "\t" << (*s).second << std::endl;
	}
};

typedef void (*arg_parse)(const char*);

static std::map<std::string, arg_parse> cprintf_args_create(void)
{
	std::map<std::string, arg_parse> ret;

	ret["log_level"] = &log::set_log_level;
	ret["printf"] = &printfun::parse_printfun;

	return ret;
}
const static std::map<std::string, arg_parse>
cprintf_args = cprintf_args_create();

static int parse_parameters(struct plugin_name_args *info)
{
	for (int i = 0; i < info->argc; i++) {
		try {
			arg_parse f = cprintf_args.at(info->argv[i].key);
			f(info->argv[i].value);
		} catch (const std::out_of_range &oor) {
			log::err << "Unknown parameter `" << info->argv[i].key
				<< "', terminating\n";
			return 1;
		} catch (const std::logic_error &le) {
			log::err << "Parse parameter `" << info->argv[i].key
				<< "' failed:\n" << le.what() << std::endl;
			return 1;
		}
	}

	if (printfun::printfuns.size() == 0) {
		/* no printf arg */
		log::err << "Specify `printf' argument with function specification\n";
		return 1;
	}

	return 0;
}

#include <tree.h>
#include <tree-pass.h>
#include <context.h>
#include <gimple.h>
#include <gimple-iterator.h>
#include <gimple-walk.h>
namespace gcc_hell {
	const pass_data init_pass_data = {
		GIMPLE_PASS,
		"cprintf_walk",
		OPTGROUP_NONE, TV_NONE,
		PROP_gimple_any,	/* properties_required */
		0,			/* properties_provided */
		0,			/* properties_destroyed */
		0,			/* todo_flags_start */
		0			/* todo_flags_finish */
	};

	struct cprintf_pass : gimple_opt_pass
	{
		cprintf_pass(gcc::context *ctx)
			: gimple_opt_pass(init_pass_data, ctx) {}

		virtual unsigned int execute(function *fun) override
		{
			struct walk_stmt_info walk_stmt_info;

			log::debug << "*** cprintf walk for function `"
				<< function_name(fun) << "' at "
				<< LOCATION_FILE(fun->function_start_locus)
				<< ":"
				<< LOCATION_LINE(fun->function_start_locus)
				<< std::endl;

			memset(&walk_stmt_info, 0, sizeof(walk_stmt_info));
			walk_gimple_seq_mod(&fun->gimple_body, callback_stmt,
					callback_op, &walk_stmt_info);

			return 0;
		}

		virtual cprintf_pass* clone() override
		{
			return this; /* do not clone ourselves */
		}
	private:
		static tree callback_stmt(gimple_stmt_iterator *gsi,
			bool *handled_all_ops, struct walk_stmt_info *wi)
		{
			gimple *g = gsi_stmt(*gsi);
			location_t l = gimple_location(g);
			enum gimple_code code = gimple_code(g);

			log::debug << "\tCallback for statement: `"
				<< gimple_code_name[code] << "' at "
				<< LOCATION_FILE(l) << ":"
				<< LOCATION_LINE(l) << std::endl;

			return NULL;
		}

		static tree callback_op(tree *t, int *, void *data)
		{
			enum tree_code code = TREE_CODE(*t);

			log::debug << "\tCallback for operand `"
				<< get_tree_code_name(code) << "'\n";

			return NULL;
		}
	};
};

int plugin_init(struct plugin_name_args *info,
		struct plugin_gcc_version *version)
{
	struct register_pass_info pass_info;
	int ret;

	/*
	 * Check the current gcc loading this plugin against the gcc,
	 * used to compile this plugin.
	 */
	if (!plugin_default_version_check(version, &gcc_version)) {
		log::err << "This GCC plugin is for version " <<
			GCCPLUGIN_VERSION_MAJOR << "." <<
			GCCPLUGIN_VERSION_MINOR << std::endl;
		return 1;
	}

	ret = parse_parameters(info);
	if (ret) {
		log::err << "Failed to parse plugin parameters: "
			<< ret << std::endl;
		return ret;
	}

	/*
	 * Register cprintf pass before building CFG, otherwise
	 * fun->gimple_body is not accessible anymore.
	 */
	pass_info.pass = new gcc_hell::cprintf_pass(g);
	pass_info.reference_pass_name = "cfg";
	pass_info.ref_pass_instance_number = 1;
	pass_info.pos_op = PASS_POS_INSERT_BEFORE;

	register_callback(info->base_name, PLUGIN_PASS_MANAGER_SETUP,
			NULL, &pass_info);

	return 0;
}
