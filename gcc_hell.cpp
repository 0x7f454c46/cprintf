#include <vector>
#include "log.h"
#include "gcc_hell.h"
#include "printfun.h"

namespace gcc_hell {

const size_t prefer_puts = 1;

static const pass_data init_pass_data = {
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

cprintf_pass::cprintf_pass(gcc::context *ctx)
	: gimple_opt_pass(init_pass_data, ctx)
{
}

unsigned int cprintf_pass::execute(function *fun)
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

tree cprintf_pass::callback_op(tree *t, int *, void *data)
{
	return NULL;
}


static inline bool func_is_printfun(const char *fun)
{
	return printfun::printfuns.find(fun) != printfun::printfuns.end();
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

			for (; i < TREE_OPERAND_LENGTH(var); i++) {
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

static inline tree printfun_get_const_fmt(gcall *stmt, const char *func_name)
{
	tree fmt_str;
	printfun::printfun_t &pf = printfun::printfuns.at(func_name);

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


static void handle_printfunc(gimple_stmt_iterator *gsi,
	struct walk_stmt_info *wi, gcall *stmt,
	const char *func_name, tree const_fmt);

tree cprintf_pass::callback_stmt(gimple_stmt_iterator *gsi,
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

static void insert_spec_func(printfun::printfun_t &pf,
		gcall *printf_stmt, gimple_stmt_iterator *gsi,
		size_t cur_spec, std::pair<std::string, bool> token);

static void handle_printfunc(gimple_stmt_iterator *gsi,
		struct walk_stmt_info *wi, gcall *stmt,
		const char *func_name, tree const_fmt)
{
	const char *fmt = TREE_STRING_POINTER(const_fmt);
	std::vector<std::pair<std::string, bool>> tokens;
	gimple *g = gsi_stmt(*gsi);
	printfun::printfun_t &pf = printfun::printfuns.at(func_name);

	log::info << "\t\tTrying to handle `" << func_name << "' call";
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
			build_spec_function(pf, printf_stmt, cur_spec, token);
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
		spec_args[pf.fmt_pos] = gimple_call_arg(printf_stmt, token_arg);
	} else {
		/* XXX: handle prefer_puts > 1 */
		if (token.first.length() <= prefer_puts &&
				pf.spec_to_func.find("c") != pf.spec_to_func.end()) {
			tree f = build_int_cst(char_type_node, token.first[0]);
			spec_args[pf.fmt_pos] = f;
		} else if (pf.spec_to_func.find("%") != pf.spec_to_func.end()) {
			/* const char *ptr, size_t size, size_t nmemb */
			spec_args.safe_grow_cleared(pf.fmt_pos + 3);
			std::string &s = token.first;
			tree fmt = build_string(s.length() + 1, s.c_str());
			tree size = build_int_cst(size_type_node, 1);
			tree nmemb = build_int_cst(size_type_node, s.length());
			fmt = create_string_param(fmt);
			spec_args[pf.fmt_pos] = fmt;
			spec_args[pf.fmt_pos + 1] = size;
			spec_args[pf.fmt_pos + 2] = nmemb;
		} else {
			std::string &s = token.first;
			tree fmt_part = build_string(s.length() + 1, s.c_str());
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

}; /* namespace gcc_hell */

