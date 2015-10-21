/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2014  Davide Mendolia, Patrick Allaert            |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Authors: Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/

#include "php_apm.h"
#include "php.h"
#include "zend_types.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_string.h"

ZEND_DECLARE_MODULE_GLOBALS(apm);

static void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str);
static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth);
static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str, char is_object, char depth);
static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC);
static int append_variable(zval *expr, smart_str *trace_str);
static char *apm_addslashes(char *str, uint length, int *new_length);

void append_backtrace(smart_str *trace_str TSRMLS_DC)
{
	/* backtrace variables */
	zend_execute_data *ptr, *skip, *prev;
	int lineno;
	const char *function_name;
	const char *filename;
	const char *class_name = NULL;
	const char *free_class_name = NULL;
	char *call_type;
	const char *include_filename = NULL;
	zval *arg_array = NULL;
#if PHP_API_VERSION < 20090626
	void **cur_arg_pos = EG(argument_stack).top_element;
	void **args = cur_arg_pos;
	int arg_stack_consistent = 0;
	int frames_on_stack = 0;
#endif
	int indent = 0;
	zend_uint class_name_len = 0;
	int dup;

#if PHP_API_VERSION < 20090626
	while (--args > EG(argument_stack).elements) {
		if (*args--) {
			break;
		}
		args -= *(ulong*)args;
		frames_on_stack++;

		/* skip args from incomplete frames */
		while (((args-1) > EG(argument_stack).elements) && *(args-1)) {
			args--;
		}

		if ((args-1) == EG(argument_stack).elements) {
			arg_stack_consistent = 1;
			break;
		}
	}
#endif
	ptr = EG(current_execute_data);

	while (ptr) {
		class_name = call_type = NULL;
		arg_array = NULL;

		skip = ptr;
		/* skip internal handler */
		if (!skip->op_array &&
			skip->prev_execute_data &&
			skip->prev_execute_data->opline &&
			skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL &&
			skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL_BY_NAME &&
			skip->prev_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
			skip = skip->prev_execute_data;
		}

		if (skip->op_array) {
			filename = skip->op_array->filename;
			lineno = skip->opline->lineno;
		} else {
			filename = NULL;
			lineno = 0;
		}

		function_name = ptr->function_state.function->common.function_name;

		if (function_name) {
			if (ptr->object) {
				if (ptr->function_state.function->common.scope) {
					class_name = ptr->function_state.function->common.scope->name;
					class_name_len = strlen(class_name);
				} else {
					dup = zend_get_object_classname(ptr->object, &class_name, &class_name_len TSRMLS_CC);
					if(!dup) {
						free_class_name = class_name;
					}
				}

				call_type = "->";
			} else if (ptr->function_state.function->common.scope) {
				class_name = ptr->function_state.function->common.scope->name;
				class_name_len = strlen(class_name);
				call_type = "::";
			} else {
				class_name = NULL;
				call_type = NULL;
			}
			if ((! ptr->opline) || ((ptr->opline->opcode == ZEND_DO_FCALL_BY_NAME) || (ptr->opline->opcode == ZEND_DO_FCALL))) {
#if PHP_API_VERSION >= 20090626
				if (ptr->function_state.arguments) {
					arg_array = debug_backtrace_get_args(&ptr->function_state.arguments TSRMLS_CC);
				}
#else
				if (arg_stack_consistent && (frames_on_stack > 0)) {
					arg_array = debug_backtrace_get_args(&cur_arg_pos TSRMLS_CC);
					frames_on_stack--;
				}
#endif
			}
		} else {
			/* i know this is kinda ugly, but i'm trying to avoid extra cycles in the main execution loop */
			zend_bool build_filename_arg = 1;

			if (!ptr->opline || ptr->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
				/* can happen when calling eval from a custom sapi */
				function_name = "unknown";
				build_filename_arg = 0;
			} else
#if ZEND_MODULE_API_NO >= 20100409 /* ZE2.4 */
			switch (ptr->opline->op2.constant) {
#else
			switch (Z_LVAL(ptr->opline->op2.u.constant)) {
#endif
				case ZEND_EVAL:
					function_name = "eval";
					build_filename_arg = 0;
					break;
				case ZEND_INCLUDE:
					function_name = "include";
					break;
				case ZEND_REQUIRE:
					function_name = "require";
					break;
				case ZEND_INCLUDE_ONCE:
					function_name = "include_once";
					break;
				case ZEND_REQUIRE_ONCE:
					function_name = "require_once";
					break;
				default:
					/* this can actually happen if you use debug_backtrace() in your error_handler and
					 * you're in the top-scope */
					function_name = "unknown";
					build_filename_arg = 0;
					break;
			}

			if (build_filename_arg && include_filename) {
				MAKE_STD_ZVAL(arg_array);
				array_init(arg_array);
				add_next_index_string(arg_array, include_filename, 1);
			}
			call_type = NULL;
		}
		smart_str_appendc(trace_str, '#');
		smart_str_append_long(trace_str, indent);
		smart_str_appendc(trace_str, ' ');
		if (class_name) {
			smart_str_appendl(trace_str, class_name, class_name_len);
			/* here, call_type is either "::" or "->" */
			smart_str_appendl(trace_str, call_type, 2);
		}
		if (function_name) {
			smart_str_appends(trace_str, function_name);
		} else {
			smart_str_appendl(trace_str, "main", 4);
		}
		smart_str_appendc(trace_str, '(');
		if (arg_array) {
			debug_print_backtrace_args(arg_array TSRMLS_CC, trace_str);
			zval_ptr_dtor(&arg_array);
		}
		if (filename) {
			smart_str_appendl(trace_str, ") called at [", sizeof(") called at [") - 1);
			smart_str_appends(trace_str, filename);
			smart_str_appendc(trace_str, ':');
			smart_str_append_long(trace_str, lineno);
			smart_str_appendl(trace_str, "]\n", 2);
		} else {
			prev = skip->prev_execute_data;

			while (prev) {
				if (prev->function_state.function &&
					prev->function_state.function->common.type != ZEND_USER_FUNCTION) {
					prev = NULL;
					break;
				}
				if (prev->op_array) {
					smart_str_appendl(trace_str, ") called at [", sizeof(") called at [") - 1);
					smart_str_appends(trace_str, prev->op_array->filename);
					smart_str_appendc(trace_str, ':');
					smart_str_append_long(trace_str, (long) prev->opline->lineno);
					smart_str_appendl(trace_str, "]\n", 2);
					break;
				}
				prev = prev->prev_execute_data;
			}
			if (!prev) {
				smart_str_appendl(trace_str, ")\n", 2);
			}
		}
		include_filename = filename;
		ptr = skip->prev_execute_data;
		++indent;
		if (free_class_name) {
			efree((char *) free_class_name);
			free_class_name = NULL;
		}
	}
}

static void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str)
{
	zval **tmp;
	HashPosition iterator;
	int i = 0;

	zend_hash_internal_pointer_reset_ex(arg_array->value.ht, &iterator);
	while (zend_hash_get_current_data_ex(arg_array->value.ht, (void **) &tmp, &iterator) == SUCCESS) {
		if (i++) {
			smart_str_appendl(trace_str, ", ", 2);
		}
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str, 0);
		zend_hash_move_forward_ex(arg_array->value.ht, &iterator);
	}
}


static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth)
{
	HashTable *properties = NULL;
	char *class_name = NULL;
	zend_uint clen;

	if (depth >= APM_G(dump_max_depth)) {
		smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
		return;
	}

	switch (Z_TYPE_P(expr)) {
		case IS_ARRAY:
			smart_str_appendl(trace_str, "array(", 6);
			if (++Z_ARRVAL_P(expr)->nApplyCount>1) {
				smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
				Z_ARRVAL_P(expr)->nApplyCount--;
				return;
			}
			append_flat_hash(Z_ARRVAL_P(expr) TSRMLS_CC, trace_str, 0, depth + 1);
			smart_str_appendc(trace_str, ')');
			Z_ARRVAL_P(expr)->nApplyCount--;
			break;
		case IS_OBJECT:
		{
			if (Z_OBJ_HANDLER_P(expr, get_class_name)) {
				Z_OBJ_HANDLER_P(expr, get_class_name)(expr, (const char **) &class_name, &clen, 0 TSRMLS_CC);
			}
			if (class_name) {
				smart_str_appendl(trace_str, class_name, clen);
				smart_str_appendl(trace_str, " Object (", sizeof(" Object (") - 1);
			} else {
				smart_str_appendl(trace_str, "Unknown Class Object (", sizeof("Unknown Class Object (") - 1);
			}
			if (class_name) {
				efree(class_name);
			}
			if (Z_OBJ_HANDLER_P(expr, get_properties)) {
				properties = Z_OBJPROP_P(expr);
			}
			if (properties) {
				if (++properties->nApplyCount>1) {
					smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
					properties->nApplyCount--;
					return;
				}
				append_flat_hash(properties TSRMLS_CC, trace_str, 1, depth + 1);
				properties->nApplyCount--;
			}
			smart_str_appendc(trace_str, ')');
			break;
		}
		default:
			append_variable(expr, trace_str);
			break;
	}
}

static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str, char is_object, char depth)
{
	zval **tmp;
	char *string_key;
	char * temp;
	HashPosition iterator;
	ulong num_key;
	uint str_len;
	int i = 0;
	int new_len;

	zend_hash_internal_pointer_reset_ex(ht, &iterator);
	while (zend_hash_get_current_data_ex(ht, (void **) &tmp, &iterator) == SUCCESS) {
		if (depth >= APM_G(dump_max_depth)) {
			smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
			return;
		}

		if (i++ > 0) {
			smart_str_appendl(trace_str, ", ", 2);
		}
		switch (zend_hash_get_current_key_ex(ht, &string_key, &str_len, &num_key, 0, &iterator)) {
			case HASH_KEY_IS_STRING:
				if (is_object) {
					if (*string_key == '\0') {
						do {
							++string_key;
							--str_len;
						} while (*(string_key) != '\0');
						++string_key;
						--str_len;
					}
				}
				smart_str_appendc(trace_str, '"');

				if (str_len > 0) {
					temp = apm_addslashes(string_key, str_len - 1, &new_len);
					smart_str_appendl(trace_str, temp, new_len);
					if (temp) {
						efree(temp);
					}
				}
				else
				{
					smart_str_appendl(trace_str, "*unknown key*", sizeof("*unknown key*") - 1);
				}

				smart_str_appendc(trace_str, '"');
				break;
			case HASH_KEY_IS_LONG:
				smart_str_append_long(trace_str, (long) num_key);
				break;
		}
		smart_str_appendl(trace_str, " => ", 4);
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str, depth);
		zend_hash_move_forward_ex(ht, &iterator);
	}
}

static int append_variable(zval *expr, smart_str *trace_str)
{
	zval expr_copy;
	int use_copy;
	char is_string = 0;
	char * temp;
	int new_len;

	if (Z_TYPE_P(expr) == IS_STRING) {
		smart_str_appendc(trace_str, '"');
		is_string = 1;
	}

	zend_make_printable_zval(expr, &expr_copy, &use_copy);
	if (use_copy) {
		expr = &expr_copy;
	}
	if (Z_STRLEN_P(expr) == 0) { /* optimize away empty strings */
		if (is_string) {
			smart_str_appendc(trace_str, '"');
		}
		if (use_copy) {
			zval_dtor(expr);
		}
		return 0;
	}

	if (is_string) {
		temp = apm_addslashes(Z_STRVAL_P(expr), Z_STRLEN_P(expr), &new_len);
		smart_str_appendl(trace_str, temp, new_len);
		smart_str_appendc(trace_str, '"');
		if (temp) {
			efree(temp);
		}
	} else {
		smart_str_appendl(trace_str, Z_STRVAL_P(expr), Z_STRLEN_P(expr));
	}

	if (use_copy) {
		zval_dtor(expr);
	}
	return Z_STRLEN_P(expr);
}

static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC)
{
#if PHP_API_VERSION >= 20090626
	void **p = *curpos;
#else
	void **p = *curpos - 2;
#endif
	zval *arg_array, **arg;
	int arg_count = 
#if PHP_VERSION_ID >= 50202
	(int)(zend_uintptr_t)
#else
	(ulong)
#endif
	*p;
#if PHP_API_VERSION < 20090626
	*curpos -= (arg_count+2);

#endif
	MAKE_STD_ZVAL(arg_array);
#if PHP_API_VERSION >= 20090626
	array_init_size(arg_array, arg_count);
#else
	array_init(arg_array);
#endif
	p -= arg_count;

	while (--arg_count >= 0) {
		arg = (zval **) p++;
		if (*arg) {
			if (Z_TYPE_PP(arg) != IS_OBJECT) {
				SEPARATE_ZVAL_TO_MAKE_IS_REF(arg);
			}
#if PHP_API_VERSION >= 20090626
			Z_ADDREF_PP(arg);
#else
			(*arg)->refcount++;
#endif
			add_next_index_zval(arg_array, *arg);
		} else {
			add_next_index_null(arg_array);
		}
	}

#if PHP_API_VERSION < 20090626
	/* skip args from incomplete frames */
	while ((((*curpos)-1) > EG(argument_stack).elements) && *((*curpos)-1)) {
		(*curpos)--;
	}

#endif
	return arg_array;
}

static char *apm_addslashes(char *str, uint length, int *new_length)
{
	/* maximum string length, worst case situation */
	char *new_str;
	char *source, *target;
	char *end;
	int local_new_length;

	if (!new_length) {
		new_length = &local_new_length;
	}

	if (!str) {
		*new_length = 0;
		return str;
	}
	new_str = (char *) safe_emalloc(2, (length ? length : (length = strlen(str))), 1);
	source = str;
	end = source + length;
	target = new_str;

	while (source < end) {
		switch (*source) {
			case '\0':
				*target++ = '\\';
				*target++ = '0';
				break;
			case '\"':
			case '\\':
				*target++ = '\\';
				/* break is missing *intentionally* */
			default:
				*target++ = *source;
				break;
		}

		source++;
	}

	*target = 0;
	*new_length = target - new_str;
	return (char *) erealloc(new_str, *new_length + 1);
}
