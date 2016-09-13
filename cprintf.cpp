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
		std::map<std::string, tree>		spec_to_tree;
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
	const size_t prefer_puts = 1;
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
	static tree create_string_param(tree string)
	{
		tree i_type, a_type;
		const int length = TREE_STRING_LENGTH(string);

		gcc_assert(length > 0);

		i_type = build_index_type(build_int_cst(NULL_TREE, length - 1));
		a_type = build_array_type(char_type_node, i_type);

		TREE_TYPE(string) = a_type;
		TREE_CONSTANT(string) = 1;
		TREE_READONLY(string) = 1;

		return build1(ADDR_EXPR, ptr_type_node, string);
	}

	struct cprintf_pass : gimple_opt_pass
	{
		cprintf_pass(gcc::context *ctx)
			: gimple_opt_pass(init_pass_data, ctx) {}

		virtual unsigned int execute(function *fun) override
		{
			struct walk_stmt_info walk_stmt_info;

			log::info << "*** cprintf walk for function `"
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
			const char *func_name;
			gcall *call_stmt;
			tree fndecl;
			tree const_fmt;

			/* Interested only in printf-alike function calls */
			if (!is_gimple_call(g))
				return NULL;

			call_stmt = dyn_cast<gcall *>(g);
			fndecl = gimple_call_fndecl(call_stmt);

			if (fndecl == NULL_TREE)
				return NULL;

			func_name = get_name(fndecl);

			log::debug << "\tCall to function `" << func_name << "'";
			if (gimple_has_location(g))
				log::debug << " at " << gimple_filename(g)
					<< ":" << gimple_lineno(g);
			log::debug << std::endl;

			if (!func_is_printfun(func_name))
				return NULL;

			log::debug << "\tChecking `"
			       << func_name << "' for constant fmt string\n";

			const_fmt = printfun_get_const_fmt(call_stmt, func_name);
			if (const_fmt == NULL_TREE)
				return NULL;

			handle_printfunc(gsi, wi, call_stmt,
					func_name, const_fmt);

			return NULL;
		}

		static tree callback_op(tree *t, int *, void *data)
		{
			return NULL;
		}

		static inline bool func_is_printfun(const char *fun)
		{
			return printfun::printfuns.find(fun) !=
				printfun::printfuns.end();
		}

		static inline tree printfun_get_const_fmt(gcall *stmt,
				const char *func_name)
		{
			tree fmt_str;
			printfun::printfun_t &pf =
				printfun::printfuns.at(func_name);

			if (gimple_call_num_args(stmt) <= pf.fmt_pos)
				return NULL_TREE;

			fmt_str = gimple_call_arg(stmt, pf.fmt_pos);
			fmt_str = get_string_cst(fmt_str);

			if (fmt_str == NULL_TREE)
				return NULL_TREE;

			if (!TREE_CONSTANT(fmt_str))
				return NULL_TREE;

			return fmt_str;
		}

		static tree get_string_cst(tree var)
		{
			if (var == NULL_TREE)
				return NULL_TREE;

			if (TREE_CODE(var) == STRING_CST)
				return var;

			switch (TREE_CODE_CLASS(TREE_CODE(var))) {
				case tcc_expression:
				case tcc_reference: {
					int i = 0;
					for (; i < TREE_OPERAND_LENGTH(var); i++)
					{
						tree ret = TREE_OPERAND(var, i);
						ret = get_string_cst(ret);
						if (ret != NULL_TREE)
							return ret;
					}
					break;
				}
				default:
					break;
			}

			return NULL_TREE;
		}

		static void handle_printfunc(gimple_stmt_iterator *gsi,
			struct walk_stmt_info *wi, gcall *stmt,
			const char *func_name, tree const_fmt)
		{
			const char *fmt = TREE_STRING_POINTER(const_fmt);
			std::vector<std::pair<std::string, bool>> tokens;
			gimple *g = gsi_stmt(*gsi);
			printfun::printfun_t &pf =
				printfun::printfuns.at(func_name);

			log::info << "\t\tTrying to handle `"
				<< func_name << "' call";
			if (gimple_has_location(g))
				log::info << " at " << gimple_filename(g)
					<< ":" << gimple_lineno(g);
			log::info << std::endl;

			tokens = tokens_create(fmt, pf);
			if (tokens.size() == 0) {
				if (gimple_has_location(g))
					log::warn << "\t\tIgnoring format string at:"
						<< gimple_filename(g) << ":"
						<< gimple_lineno(g) << "\n";
				return;
			}
			log::debug << "\t\tTokens from format string: ";
			for (std::vector<std::pair<std::string, bool>>::iterator i =
					tokens.begin(); i != tokens.end(); ++i) {
				if (i->second)
					log::debug << "%" << i->first << ", ";
				else
					log::debug << "`" << (*i).first << "', ";
			}
			log::debug << std::endl;

			size_t specs = 0;
			for (size_t i = 0; i < tokens.size(); ++i) {
				if (tokens[i].second)
					specs++;
				insert_spec_func(pf, stmt, gsi, specs, tokens[i]);
			}
			gsi_remove(gsi, true);
		}

		static std::vector<std::pair<std::string, bool>>
		tokens_create(const char *fmt, const printfun::printfun_t &pf)
		{
			std::vector<std::pair<std::string, bool>> ret;
			std::string token;
			/* Do we have %s-function? */
			bool can_handle_strings =
				specifier_search("s", pf).length();

			while (*fmt != '\0') {
				if (*fmt != '%') {
					token += *fmt++;
					if (!can_handle_strings)
						goto ret_empty_str;
					continue;
				}
				/* escaped '%' symbol */
				if (*(fmt + 1) == '%') {
					token += '%';
					fmt += 2;
					if (!can_handle_strings)
						goto ret_empty_str;
					continue;
				}
				fmt++;
				if (token.length()) {
					ret.push_back(std::make_pair(token,false));
					token.clear();
				}
				token = specifier_search(fmt, pf);
				if (token.length() == 0) {
					log::warn << "\t\tThis specifier wasn't defined in plugin parameters: `"
						<< "%" << fmt << "'\n";
					goto ret_empty_str;
				}
				ret.push_back(std::make_pair(token,true));
				fmt += token.length();
				token.clear();
			}

			if (token.length()) {
				if (!can_handle_strings)
					goto ret_empty_str;
				ret.push_back(std::make_pair(token,false));
			}
			return ret;

ret_empty_str:
			return std::vector<std::pair<std::string,bool>>();
		}

		static std::string specifier_search(const char *fmt,
			const printfun::printfun_t &pf)
		{
			typedef std::map<std::string,std::string>::const_iterator si;
			std::string spec;
			std::string ret;

			while (*fmt != '\0') {
				std::pair<si,si> range;
				std::string next_str;

				spec += *fmt++;
				if (pf.spec_to_func.find(spec) != pf.spec_to_func.end()) {
					ret = spec;
					continue;
				}

				range = pf.spec_to_func.equal_range(spec);
				next_str = range.first->first;
				if (next_str.compare(0, spec.length(), spec))
					break;
			}

			return ret;
		}

		static void insert_spec_func(printfun::printfun_t &pf,
			gcall *printf_stmt, gimple_stmt_iterator *gsi,
			size_t cur_spec, std::pair<std::string, bool> token)
		{
			tree spec_fn;
			vec<tree> spec_args;
			gimple *inserted;

			if (gimple_call_num_args(printf_stmt) <= pf.fmt_pos)
				/* Should never happen ;-) */
				throw std::logic_error("Internal cprintf plugin error: number of arguments in printf-like function is larger than constant string fmt parameter\n");

			if (token.second) {
				/*
				 * We checked that already while splitting
				 * fmt string, but let's be cautious
				 */
				if (pf.spec_to_func.find(token.first) == pf.spec_to_func.end())
					throw std::logic_error("Internal cprintf plugin error: found unknown specifier after splitting fmt string\n");

				if (pf.spec_to_tree.find(token.first) == pf.spec_to_tree.end())
					build_spec_function(pf, printf_stmt,
							cur_spec, token);
				spec_fn = pf.spec_to_tree.at(token.first);
			} else {
				if (token.first.length() <= prefer_puts &&
						pf.spec_to_func.find("c") != pf.spec_to_func.end()) {
					if (pf.spec_to_tree.find("c") == pf.spec_to_tree.end())
						build_spec_function(pf, printf_stmt,
								cur_spec, token);
					spec_fn = pf.spec_to_tree.at("c");
				} else if (pf.spec_to_func.find("%") != pf.spec_to_func.end()) {
					if (pf.spec_to_tree.find("%") == pf.spec_to_tree.end())
						build_spec_function(pf, printf_stmt,
								cur_spec, token);
					spec_fn = pf.spec_to_tree.at("%");
				} else {
					if (pf.spec_to_func.find("s") == pf.spec_to_func.end())
						throw std::logic_error("Internal cprintf plugin error: found constant string to print without %s-specifier handler\n");
					if (pf.spec_to_tree.find("s") == pf.spec_to_tree.end())
						build_spec_function(pf, printf_stmt,
								cur_spec, token);
					spec_fn = pf.spec_to_tree.at("s");
				}
			}

			/* Don't handle multi-arg spec handlers for now */
			spec_args.create(pf.fmt_pos + 1);
			spec_args.safe_grow_cleared(pf.fmt_pos + 1);
			for (unsigned int i = 0; i < pf.fmt_pos; ++i)
				spec_args[i] = gimple_call_arg(printf_stmt, i);
			if (token.second) {
				/*
				 * XXX: check for %s + const string parameter
				 * and combine it with format-string + fwrite()
				 */
				unsigned token_arg = pf.fmt_pos + cur_spec;
				spec_args[pf.fmt_pos] =
					gimple_call_arg(printf_stmt, token_arg);
			} else {
				/* XXX: handle prefer_puts > 1 */
				if (token.first.length() <= prefer_puts &&
						pf.spec_to_func.find("c") != pf.spec_to_func.end()) {
					tree f = build_int_cst(char_type_node,
							token.first[0]);
					spec_args[pf.fmt_pos] = f;
				} else if (pf.spec_to_func.find("%") != pf.spec_to_func.end()) {
					/* const char *ptr, size_t size, size_t nmemb */
					spec_args.safe_grow_cleared(pf.fmt_pos + 3);
					std::string &s = token.first;
					tree fmt = build_string(s.length() + 1, s.c_str());
					tree size = build_int_cst(size_type_node, 1);
					tree nmemb = build_int_cst(size_type_node,
							s.length());
					fmt = create_string_param(fmt);
					spec_args[pf.fmt_pos] = fmt;
					spec_args[pf.fmt_pos + 1] = size;
					spec_args[pf.fmt_pos + 2] = nmemb;
				} else {
					std::string &s = token.first;
					tree fmt_part =
						build_string(s.length() + 1, s.c_str());
					fmt_part = create_string_param(fmt_part);
					spec_args[pf.fmt_pos] = fmt_part;
				}
			}
			inserted = gimple_build_call_vec(spec_fn, spec_args);
			spec_args.release();
			gsi_insert_before(gsi, inserted, GSI_SAME_STMT);

			log::info << "\t\tInserted call to `";
			if (token.second) {
				log::info << pf.spec_to_func.at(token.first);
			} else {
				if (token.first.length() <= prefer_puts &&
						pf.spec_to_func.find("c") != pf.spec_to_func.end())
					log::info << pf.spec_to_func.at("c");
				else if (pf.spec_to_func.find("%") != pf.spec_to_func.end())
					log::info << pf.spec_to_func.at("%");
				else
					log::info << pf.spec_to_func.at("s");
				log::info << "(\"" << token.first << "\")";
			}
			log::info << "' function\n";
		}

		static void build_spec_function(printfun::printfun_t &pf,
			gcall *printf_stmt, size_t cur_spec,
			std::pair<std::string, bool> token)
		{
			std::vector<tree> args;
			tree fntype;
			tree func_decl;
			std::string func_name;

			for (unsigned int i = 0; i < pf.fmt_pos; ++i) {
				tree arg_n = gimple_call_arg(printf_stmt, i);
				args.push_back(TREE_TYPE(arg_n));
			}

			if (token.second) {
				tree spec_param = gimple_call_arg(printf_stmt,
						pf.fmt_pos + cur_spec);
				args.push_back(TREE_TYPE(spec_param));
				func_name = pf.spec_to_func.at(token.first);
			} else {
				tree const_char_ptr_type_node =
					build_pointer_type(build_type_variant(char_type_node, 1, 0));
				if (token.first.length() <= prefer_puts &&
						pf.spec_to_func.find("c") != pf.spec_to_func.end()) {
					args.push_back(char_type_node);
					func_name = pf.spec_to_func.at("c");
				} else if (pf.spec_to_func.find("%") != pf.spec_to_func.end()) {
					args.push_back(const_char_ptr_type_node);
					args.push_back(size_type_node);
					args.push_back(size_type_node);
					func_name = pf.spec_to_func.at("%");
				} else {
					args.push_back(const_char_ptr_type_node);
					func_name = pf.spec_to_func.at("s");
				}
			}

			/*
			 * Function return type is void for now.
			 * &args[0] is contiguos array - that's guaranteed
			 * now by C++ spec, 23.3.11
			 */
			fntype = build_function_type_array(void_type_node,
					pf.fmt_pos + 1, &args[0]);
			func_decl = build_fn_decl(func_name.c_str(), fntype);
			if (token.second) {
				pf.spec_to_tree[token.first] = func_decl;
			} else {
				if (token.first.length() <= prefer_puts &&
						pf.spec_to_func.find("c") != pf.spec_to_func.end())
					pf.spec_to_tree["c"] = func_decl;
				else if (pf.spec_to_func.find("%") != pf.spec_to_func.end())
					pf.spec_to_tree["%"] = func_decl;
				else
					pf.spec_to_tree["s"] = func_decl;
			}
			TREE_PUBLIC(func_decl)		= 1;
			DECL_EXTERNAL(func_decl)	= 1;
			DECL_ARTIFICIAL(func_decl)	= 1;
			TREE_USED(func_decl)		= 1;
			log::debug << "\t\tBuilded declaration for `" <<
				func_name << "'\n";
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
