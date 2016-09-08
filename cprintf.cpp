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

struct cprintf_func_t {
	std::string				function;
	unsigned int				fmt_position;
	std::map<std::string, std::string>	specifier_to_func;
};

struct cprintf_opts_t {
	std::vector<cprintf_func_t> printfun;
} cprintf_opts;

typedef void (*arg_parse)(const char*);

static void arg_printf(const char *print_func_def)
{
}

static std::map<std::string, arg_parse> cprintf_args_create(void)
{
	std::map<std::string, arg_parse> ret;
	ret["log_level"] = &log::set_log_level;
	ret["printf"] = &arg_printf;
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
			log::err << "Parse parameter failed: "
				<< le.what() << std::endl;
			return 1;
		}
	}

	if (cprintf_opts.printfun.size() == 0) {
		/* set defaults */
	}

	return 0;
}

int plugin_init(struct plugin_name_args *info,
		struct plugin_gcc_version *version)
{
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

	return 0;
}
