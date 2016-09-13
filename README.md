# cprintf: Compile-time parsing for printf-like function's fmt

This is GCC plugin, made to eliminate cost for parsing printf-like format strings in runtime by doing this at compile time.
It uses GCC plugin API to walk through gimple tree, get format-string value and insert calls to specified
to plugin functions instead of printf-alike. Note: _this can be done only to constant format-string
parameters_.

So, the idea is to change the function calls like this:
```
print_on_level(LOG_DEBUG, "cwd:%x swd:%x twd:%x fop:%x mxcsr:%x mxcsr_mask:%x\n",
                (int)i387->cwd, (int)i387->swd, (int)i387->twd,
                (int)i387->fop, (int)i387->mxcsr, (int)i387->mxcsr_mask);
```
To sequence of calls like:
```
print_str_level(LOG_DEBUG, "cwd:");
print_int_level(LOG_DEBUG, (int)i387->cwd);
print_str_level(LOG_DEBUG, " swd:");
print_int_level(LOG_DEBUG, (int)i387->swd);
print_str_level(LOG_DEBUG, " twd:");
print_int_level(LOG_DEBUG, (int)i387->twd);
print_str_level(LOG_DEBUG, " fop:");
print_int_level(LOG_DEBUG, (int)i387->fop);
print_str_level(LOG_DEBUG, " mxcsr:");
print_int_level(LOG_DEBUG, (int)i387->mxcsr);
print_str_level(LOG_DEBUG, " mxcsr_mask:");
print_int_level(LOG_DEBUG, (int)i387->mxcsr_mask);
print_str_level(LOG_DEBUG, "\n");
```
This will speed up the runtime by eliminating needless parsing of format string.
All is done by GCC plugin during compilation.
One should specify printf-alike function `print_on_level` and `print_str_level`, `print_int_level`
as a plugin parameters.

## TL;DR
To use this GCC compiler plugin, you need compile it with:
```
make
```
Then change your makefile so GCC will use plugin like this:
```
%.o: %.c
    gcc -o $@ $< -fplugin=./cprintf.so -fplugin-arg-cprintf-printf="printf(0): %c putchar %s putstring  \
                            %d putint %f putfloat %m puterrstr"
```
Where `printf(0)` means that format string is the first parameter.
If you provide function like `fprintf(1)` which has some (one) parameters before format string,
they will be passed to handlers in the same order.

Handlers: `putchar` function for `%c` specifier and so on.
Note, specifier may be any length, ending with space symbol. I.e., `%h$up ` is a valid specifier `h$up`.

## Reserved specifiers
For some user-defined function `foo(arg1, arg2, const char *fmt, ...)` cprintf
plugin expects that following specifiers have their special meaning if
handler-functions have been specified in plugin args:
* `%s` is for strings (const char\*). Cprintf will use it to output parts of
format string between specifiers; Function prototype in example:
`bar(arg1, arg2, const char *str);`
* `%c` is for chars (const char). Cprintf will use it to output single chars
from format string; Function prototype in example:
`baz(arg1, arg2, const char s);`
* `%%` special meaning for raw string output; Function prototype in example:
`quux(arg1, arg2, void *ptr, size_t size, size_t nmemb);`

Tip: consider using `%%` specifier as `fwrite()` function as it will
give great performance enhance.
