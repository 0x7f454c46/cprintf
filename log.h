#ifndef CPRINTF_LOG_H
#define CPRINTF_LOG_H

#include <iostream>

namespace log {

enum log_level_t {
	LOG_NONE,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_ALL
};

extern void set_log_level(const char *level);
extern log_level_t curr_log_level(void);

class log_stream {
	const log_level_t level;
public:
	log_stream(log_level_t l) : level(l) {}

	template <typename T> const log_stream& operator<<(const T& v) const
	{
		if (curr_log_level() >= level)
			std::cerr << v;
		return *this;
	}

	typedef std::basic_ostream<char, std::char_traits<char>> std_stream;
	typedef std_stream& (*std_manip)(std_stream&);
	const log_stream& operator<<(std_manip manip) const
	{
		if (curr_log_level() >= level)
			manip(std::cerr);
		return *this;
	}
};

extern log_stream err, warn, info, debug, all;

}; /* namespace log */

#endif /* CPRINTF_LOG_H */
