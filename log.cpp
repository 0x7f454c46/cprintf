#include <map>
#include <string>
#include <stdexcept>

#include "log.h"

namespace log {

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

static log_level_t log_level = LOG_WARN;

void set_log_level(const char *level)
{
	try {
		log_level = log_level_map.at(level);
	} catch (const std::out_of_range &oor) {
		std::string err("No such log level: `");
		err += level;
		throw std::logic_error(err + "'");
	}
}

log_level_t curr_log_level(void)
{
	return log_level;
}

log_stream err(LOG_ERROR), warn(LOG_WARN), info(LOG_INFO),
	   debug(LOG_DEBUG), all(LOG_ALL);

}; /* namespace log */
