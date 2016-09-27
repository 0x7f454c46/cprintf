#ifndef CPRINTF_GCC_HELL_H
#define CPRINTF_GCC_HELL_H

#include <gcc-plugin.h>
#include <tree.h>
#include <tree-pass.h>
#include <context.h>
#include <gimple.h>
#include <gimple-iterator.h>
#include <gimple-walk.h>

namespace gcc_hell {
	struct cprintf_pass : gimple_opt_pass
	{
		cprintf_pass(gcc::context *ctx);
		virtual unsigned int execute(function *fun) override;
		virtual cprintf_pass* clone() override
		{
			return this; /* do not clone ourselves */
		}
	private:
		static tree callback_stmt(gimple_stmt_iterator *gsi,
			bool *handled_all_ops, struct walk_stmt_info *wi);
		static tree callback_op(tree *t, int *, void *data);
	};
}; /* namespace gcc_hell */

#endif /* CPRINTF_GCC_HELL_H */
