#include <iostream>
#include <vector>
#include <map>

#include <gcc-plugin.h>
#include <plugin-version.h>

#include "log.h"
#include "printfun.h"
#include "gcc_hell.h"

int plugin_is_GPL_compatible = 1;

typedef void (*arg_parse)(const char*);

static std::map<std::string, arg_parse> cprintf_args_create(void)
{
	std::map<std::string, arg_parse> ret;

	ret["log_level"] = &log::set_log_level;
	ret["printf"] = &printfun::add_printfun;

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
