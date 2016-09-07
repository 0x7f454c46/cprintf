#include <iostream>

#include <gcc-plugin.h>
#include <plugin-version.h>

int plugin_is_GPL_compatible = 1;

int plugin_init(struct plugin_name_args *info,
		struct plugin_gcc_version *version)
{
	/*
	 * Check the current gcc loading this plugin against the gcc,
	 * used to compile this plugin.
	 */
	if (!plugin_default_version_check(version, &gcc_version)) {
		std::cerr << "This GCC plugin is for version " <<
			GCCPLUGIN_VERSION_MAJOR << "." <<
			GCCPLUGIN_VERSION_MINOR << std::endl;
		return 1;
	}

	return 0;
}
