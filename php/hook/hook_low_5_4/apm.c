/*
 * apm.c
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PHP_WIN32
#include "win32/time.h"
#else
#include "main/php_config.h"
#endif

#ifdef HAVE_GETRUSAGE
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "php.h"
#include "php_ini.h"
#include "zend_exceptions.h"
#include "php_apm.h"
#include "backtrace.h"
#include "ext/standard/info.h"

#ifdef APM_DRIVER_MYSQL
#include "driver_mysql.h"
#endif

#ifdef APM_DRIVER_SOCKET
#include "driver_socket.h"
#endif

//mh add for phptrace
/* Make sapi_module accessable */
extern sapi_module_struct sapi_module;

/* True global resources - no need for thread safety here */
static int le_trace;

/**
 * Trace Global
 * --------------------
 */
/* Debug output */
//#if TRACE_DEBUG_OUTPUT
//#if 1
#define PTD(format, args...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, __VA_ARGS__)
//#else
//#define PTD(format, args...)
//#endif

/* Ctrl module */
#define CTRL_IS_ACTIVE()    pt_ctrl_is_active(&PTG(ctrl), PTG(pid))
#define CTRL_SET_INACTIVE() pt_ctrl_set_inactive(&PTG(ctrl), PTG(pid))

/* Ping process */
#define PING_UPDATE() PTG(ping) = pt_time_sec()
#define PING_TIMEOUT() (pt_time_sec() > PTG(ping) + PTG(idle_timeout))

/* Profiling */
#define PROFILING_SET(p) do { \
    p.wall_time = pt_time_usec(); \
    p.cpu_time = pt_cputime_usec(); \
    p.mem = zend_memory_usage(0 TSRMLS_CC); \
    p.mempeak = zend_memory_peak_usage(0 TSRMLS_CC); \
} while (0);

/* Flags of dotrace */
#define TRACE_TO_OUTPUT (1 << 0)
#define TRACE_TO_TOOL   (1 << 1)
#define TRACE_TO_NULL   (1 << 2)

/**
 * Compatible with PHP 5.1, zend_memory_usage() is not available in 5.1.
 * AG(allocated_memory) is the value we want, but it available only when
 * MEMORY_LIMIT is ON during PHP compilation, so use zero instead for safe.
 */
#if PHP_VERSION_ID < 50200
#define zend_memory_usage(args...) 0
#define zend_memory_peak_usage(args...) 0
typedef unsigned long zend_uintptr_t;
#endif

//mh add end
ZEND_DECLARE_MODULE_GLOBALS(apm);
static PHP_GINIT_FUNCTION(apm);
static PHP_GSHUTDOWN_FUNCTION(apm);

#define APM_DRIVER_BEGIN_LOOP driver_entry = APM_G(drivers); \
		while ((driver_entry = driver_entry->next) != NULL) {

//low version support
#if PHP_VERSION_ID < 50300
typedef opcode_handler_t user_opcode_handler_t;
#endif
//low version support end

static user_opcode_handler_t _orig_begin_silence_opcode_handler = NULL;
static user_opcode_handler_t _orig_end_silence_opcode_handler = NULL;

#if PHP_VERSION_ID >= 70000
# define ZEND_USER_OPCODE_HANDLER_ARGS zend_execute_data *execute_data
# define ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU execute_data
#else
# define ZEND_USER_OPCODE_HANDLER_ARGS ZEND_OPCODE_HANDLER_ARGS
# define ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU ZEND_OPCODE_HANDLER_ARGS_PASSTHRU
#endif

static int apm_begin_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS)
{
	APM_G(currently_silenced) = 1;

	if (_orig_begin_silence_opcode_handler) {
		_orig_begin_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	return ZEND_USER_OPCODE_DISPATCH;
}

static int apm_end_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS)
{
	APM_G(currently_silenced) = 0;

	if (_orig_end_silence_opcode_handler) {
		_orig_end_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	return ZEND_USER_OPCODE_DISPATCH;
}

int apm_write(const char *str,
#if PHP_VERSION_ID >= 70000
size_t
#else
uint
#endif
length)
{
	TSRMLS_FETCH();
	smart_str_appendl(APM_G(buffer), str, length);
	smart_str_0(APM_G(buffer));
	return length;
}

void (*old_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

void apm_throw_exception_hook(zval *exception TSRMLS_DC);

static void process_event(int, int, char *, uint, char * TSRMLS_DC);

/* recorded timestamp for the request */
struct timeval begin_tp;
#ifdef HAVE_GETRUSAGE
struct rusage begin_usg;
#endif

zend_module_entry apm_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_APM_NAME,
	NULL,
	PHP_MINIT(apm),
	PHP_MSHUTDOWN(apm),
	PHP_RINIT(apm),
	PHP_RSHUTDOWN(apm),
	PHP_MINFO(apm),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_APM_VERSION,
#endif
	PHP_MODULE_GLOBALS(apm),
	PHP_GINIT(apm),
	PHP_GSHUTDOWN(apm),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_APM
	ZEND_GET_MODULE(apm)
#endif

PHP_INI_BEGIN()
	/* Boolean controlling whether the extension is globally active or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.enabled", "1", PHP_INI_SYSTEM, OnUpdateBool, enabled, zend_apm_globals, apm_globals)
	/* Application identifier, helps identifying which application is being monitored */
	STD_PHP_INI_ENTRY("baidu.apm.application_id", "My application", PHP_INI_ALL, OnUpdateString, application_id, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the event monitoring is active or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.event_enabled", "1", PHP_INI_ALL, OnUpdateBool, event_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the stacktrace should be stored or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.store_stacktrace", "1", PHP_INI_ALL, OnUpdateBool, store_stacktrace, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the ip should be stored or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.store_ip", "1", PHP_INI_ALL, OnUpdateBool, store_ip, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the cookies should be stored or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.store_cookies", "1", PHP_INI_ALL, OnUpdateBool, store_cookies, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the POST variables should be stored or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.store_post", "1", PHP_INI_ALL, OnUpdateBool, store_post, zend_apm_globals, apm_globals)
	/* Time (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("baidu.apm.stats_duration_threshold", "100", PHP_INI_ALL, OnUpdateLong, stats_duration_threshold, zend_apm_globals, apm_globals)
#ifdef HAVE_GETRUSAGE
	/* User CPU time usage (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("baidu.apm.stats_user_cpu_threshold", "100", PHP_INI_ALL, OnUpdateLong, stats_user_cpu_threshold, zend_apm_globals, apm_globals)
	/* System CPU time usage (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("baidu.apm.stats_sys_cpu_threshold", "10", PHP_INI_ALL, OnUpdateLong, stats_sys_cpu_threshold, zend_apm_globals, apm_globals)
#endif
	/* Maximum recursion depth used when dumping a variable */
	STD_PHP_INI_ENTRY("baidu.apm.dump_max_depth", "1", PHP_INI_ALL, OnUpdateLong, dump_max_depth, zend_apm_globals, apm_globals)

//file
	/* Boolean controlling whether the driver is active or not */
	//STD_PHP_INI_BOOLEAN("baidu.apm.file_enabled", "1", PHP_INI_PERDIR, OnUpdateBool, file_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	//STD_PHP_INI_BOOLEAN("baidu.apm.file_stats_enabled", "0", PHP_INI_ALL, OnUpdateBool, file_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	//STD_PHP_INI_ENTRY("baidu.apm.file_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, file_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	//STD_PHP_INI_ENTRY("baidu.apm.file_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMfileErrorReporting, file_error_reporting, zend_apm_globals, apm_globals)

#ifdef APM_DRIVER_MYSQL
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.mysql_enabled", "1", PHP_INI_PERDIR, OnUpdateBool, mysql_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("baidu.apm.mysql_stats_enabled", "0", PHP_INI_ALL, OnUpdateBool, mysql_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, mysql_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMmysqlErrorReporting, mysql_error_reporting, zend_apm_globals, apm_globals)
	/* mysql host */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_host", "localhost", PHP_INI_PERDIR, OnUpdateString, mysql_db_host, zend_apm_globals, apm_globals)
	/* mysql port */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_port", "0", PHP_INI_PERDIR, OnUpdateLong, mysql_db_port, zend_apm_globals, apm_globals)
	/* mysql user */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_user", "root", PHP_INI_PERDIR, OnUpdateString, mysql_db_user, zend_apm_globals, apm_globals)
	/* mysql password */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_pass", "", PHP_INI_PERDIR, OnUpdateString, mysql_db_pass, zend_apm_globals, apm_globals)
	/* mysql database */
	STD_PHP_INI_ENTRY("baidu.apm.mysql_db", "apm", PHP_INI_PERDIR, OnUpdateString, mysql_db_name, zend_apm_globals, apm_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("baidu.apm.mysql_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, mysql_process_silenced_events, zend_apm_globals, apm_globals)
#endif

#ifdef APM_DRIVER_SOCKET
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("baidu.apm.socket_enabled", "1", PHP_INI_ALL, OnUpdateBool, socket_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("baidu.apm.socket_stats_enabled", "1", PHP_INI_ALL, OnUpdateBool, socket_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("baidu.apm.socket_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, socket_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("baidu.apm.socket_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMsocketErrorReporting, socket_error_reporting, zend_apm_globals, apm_globals)
	/* Socket path */
	STD_PHP_INI_ENTRY("baidu.apm.socket_path", "file:/tmp/apm.sock|tcp:127.0.0.1:8264", PHP_INI_ALL, OnUpdateString, socket_path, zend_apm_globals, apm_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("baidu.apm.socket_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, socket_process_silenced_events, zend_apm_globals, apm_globals)
#endif
PHP_INI_END()

static PHP_GINIT_FUNCTION(apm)
{
	apm_driver_entry **next;
	apm_globals->buffer = NULL;
	// init drivers
	apm_globals->drivers = (apm_driver_entry *) malloc(sizeof(apm_driver_entry));
	apm_globals->drivers->driver.process_event = (void (*)(PROCESS_EVENT_ARGS)) NULL;
	apm_globals->drivers->driver.process_stats = (void (*)(TSRMLS_D)) NULL;
	apm_globals->drivers->driver.minit = (int (*)(int TSRMLS_DC)) NULL;
	apm_globals->drivers->driver.rinit = (int (*)(TSRMLS_D)) NULL;
	apm_globals->drivers->driver.mshutdown = (int (*)()) NULL;
	apm_globals->drivers->driver.rshutdown = (int (*)(TSRMLS_D)) NULL;

	next = &apm_globals->drivers->next;
	*next = (apm_driver_entry *) NULL;

	#ifdef APM_DRIVER_MYSQL
		*next = apm_driver_mysql_create();
		next = &(*next)->next;
	#endif

	#ifdef APM_DRIVER_SOCKET
		*next = apm_driver_socket_create();
		next = &(*next)->next;
	#endif
}

static void recursive_free_driver(apm_driver_entry **driver)
{
	if ((*driver)->next) {
		recursive_free_driver(&(*driver)->next);
	}
	free(*driver);
}

static PHP_GSHUTDOWN_FUNCTION(apm)
{
	recursive_free_driver(&apm_globals->drivers);
}

PHP_MINIT_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;

	REGISTER_INI_ENTRIES();

	// mh add for phptrace
 	/* Replace executor */
	#if PHP_VERSION_ID < 50500
	    pt_ori_execute = zend_execute;
	    zend_execute = pt_execute;
	#else
	    pt_ori_execute_ex = zend_execute_ex;
	    zend_execute_ex = pt_execute_ex;
	#endif
	    pt_ori_execute_internal = zend_execute_internal;
	    zend_execute_internal = pt_execute_internal;

	//mh add for phptrace end

	/* Storing actual error callback function for later restore */
	old_error_cb = zend_error_cb;

	/* Initialize the storage drivers */
	if (APM_G(enabled)) {
		/* Overload the ZEND_BEGIN_SILENCE / ZEND_END_SILENCE opcodes */
		_orig_begin_silence_opcode_handler = zend_get_user_opcode_handler(ZEND_BEGIN_SILENCE);
		zend_set_user_opcode_handler(ZEND_BEGIN_SILENCE, apm_begin_silence_opcode_handler);

		_orig_end_silence_opcode_handler = zend_get_user_opcode_handler(ZEND_END_SILENCE);
		zend_set_user_opcode_handler(ZEND_END_SILENCE, apm_end_silence_opcode_handler);

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.minit(module_number TSRMLS_CC) == FAILURE) {
				return FAILURE;
			}
		}

		/* Since xdebug looks for zend_error_cb in his MINIT, we change it once more so he can get our address */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;

	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;

	UNREGISTER_INI_ENTRIES();

	if (APM_G(enabled)) {
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.mshutdown(SHUTDOWN_FUNC_ARGS_PASSTHRU) == FAILURE) {
				return FAILURE;
			}
		}
	}

	/* Restoring saved error callback function */
	//
	zend_error_cb = old_error_cb;

	//mh add for phptrace
 	/* Restore original executor */
	#if PHP_VERSION_ID < 50500
	    zend_execute = pt_ori_execute;
	#else
	    zend_execute_ex = pt_ori_execute_ex;
	#endif
	    zend_execute_internal = pt_ori_execute_internal;

	//mh add for phptrace end

	return SUCCESS;
}

PHP_RINIT_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;

	APM_INIT_DEBUG;
	//file record
	APM_INIT_FILE_STATS_RECORD;
	APM_INIT_FILE_EVENTS_RECORD;
	APM_INIT_FILE_TRACE_RECORD;

	//mh add
	memset( APM_G(whole_trace_str), '\0', sizeof(APM_G(whole_trace_str)) );
	PTG(dotrace) = 0;
	//mh add end

	if (APM_G(enabled)) {
		//mh add
		PTG(dotrace) = 1;
		//mh add end
		memset(&APM_G(request_data), 0, sizeof(struct apm_request_data));
		if (APM_G(event_enabled)) {
			/* storing timestamp of request */
			gettimeofday(&begin_tp, NULL);
#ifdef HAVE_GETRUSAGE
			memset(&begin_usg, 0, sizeof(struct rusage));
			getrusage(RUSAGE_SELF, &begin_usg);
#endif
		}

		APM_DEBUG("Registering drivers\n");
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled(TSRMLS_C)) {
				if (driver_entry->driver.rinit(TSRMLS_C)) {
					return FAILURE;
				}
			}
		}

		APM_DEBUG("Replacing handlers\n");
		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;
	}

	// mh add for phptrace
    // pt_status_t status;
    // pt_status_build(&status, 1, EG(current_execute_data) TSRMLS_CC);
    // pt_status_display(&status TSRMLS_CC);
    // pt_status_destroy(&status TSRMLS_CC);
	//mh add end

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;
#ifdef HAVE_GETRUSAGE
	struct rusage usg;
#endif
	struct timeval end_tp;
	zend_bool stats_enabled = 0;
	int code = SUCCESS;

	// mh add for phptrace
    // pt_status_t status;
    // pt_status_build(&status, 1, EG(current_execute_data) TSRMLS_CC);
    // pt_status_display(&status TSRMLS_CC);
    // pt_status_destroy(&status TSRMLS_CC);
	//mh add end

	// 	//mh add for phptrace
 	//    /* Restore original executor */
	// #if PHP_VERSION_ID < 50500
	//     zend_execute = pt_ori_execute;
	// #else
	//     zend_execute_ex = pt_ori_execute_ex;
	// #endif
	//     zend_execute_internal = pt_ori_execute_internal;

	// //mh add for phptrace end

	if (APM_G(enabled)) {
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL && stats_enabled == 0) {
			stats_enabled = driver_entry->driver.want_stats(TSRMLS_C);
		}

		if (stats_enabled) {
			gettimeofday(&end_tp, NULL);

			/* Request longer than accepted threshold ? */
			APM_G(duration) = (float) (SEC_TO_USEC(end_tp.tv_sec - begin_tp.tv_sec) + end_tp.tv_usec - begin_tp.tv_usec);
			APM_G(mem_peak_usage) = zend_memory_peak_usage(1 TSRMLS_CC);
#ifdef HAVE_GETRUSAGE
			memset(&usg, 0, sizeof(struct rusage));

			if (getrusage(RUSAGE_SELF, &usg) == 0) {
				APM_G(user_cpu) = (float) (SEC_TO_USEC(usg.ru_utime.tv_sec - begin_usg.ru_utime.tv_sec) + usg.ru_utime.tv_usec - begin_usg.ru_utime.tv_usec);
				APM_G(sys_cpu) = (float) (SEC_TO_USEC(usg.ru_stime.tv_sec - begin_usg.ru_stime.tv_sec) + usg.ru_stime.tv_usec - begin_usg.ru_stime.tv_usec);
			}
#endif
			//mh add
			if (APM_G(duration) > 1000.0 * APM_G(stats_duration_threshold)) {
				PTG(dotrace) = 1;
			}

			//mh add end

			if (1
				//APM_G(duration) > 1000.0 * APM_G(stats_duration_threshold)
// #ifdef HAVE_GETRUSAGE
// 				|| APM_G(user_cpu) > 1000.0 * APM_G(stats_user_cpu_threshold)
// 				|| APM_G(sys_cpu) > 1000.0 * APM_G(stats_sys_cpu_threshold)
// #endif
				) {

				//mh add for phptrace
				//PTG(dotrace) = 1;
				//mh add end

				driver_entry = APM_G(drivers);
				APM_DEBUG("Stats loop begin\n");
				while ((driver_entry = driver_entry->next) != NULL) {
					if (driver_entry->driver.want_stats(TSRMLS_C)) {
						driver_entry->driver.process_stats(TSRMLS_C);
					}
				}
				// add file recore
				// delete file record
				//do_file_record_stats();

				//end file record

				APM_DEBUG("Stats loop end\n");
			}
		}

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled(TSRMLS_C)) {
				if (driver_entry->driver.rshutdown(TSRMLS_C) == FAILURE) {
					code = FAILURE;
				}
			}
		}

		/* Restoring saved error callback function */
		APM_DEBUG("Restoring handlers\n");
		zend_error_cb = old_error_cb;
		zend_throw_exception_hook = NULL;

		smart_str_free(&APM_RD(cookies));
		smart_str_free(&APM_RD(post_vars));
	}

	APM_SHUTDOWN_DEBUG;

	//shutdown record
	APM_SHUTDOWN_FILE_STATS_RECODE;
	APM_SHUTDOWN_FILE_EVENTS_RECODE;
	APM_SHUTDOWN_FILE_TRACE_RECODE;

	return code;
}

PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Baidu PHP APM support", "enabled");
	php_info_print_table_row(2, "Version", PHP_APM_VERSION);
	php_info_print_table_row(2, "Author", PHP_APM_AUTHOR);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
	This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *msg;
	va_list args_copy;
	TSRMLS_FETCH();

	/* A copy of args is needed to be used for the old_error_cb */
	va_copy(args_copy, args);
	vspprintf(&msg, 0, format, args_copy);
	va_end(args_copy);

	if (APM_G(event_enabled)) {
		process_event(APM_EVENT_ERROR, type, (char *) error_filename, error_lineno, msg TSRMLS_CC);
	}
	efree(msg);

	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */

void apm_throw_exception_hook(zval *exception TSRMLS_DC)
{
	zval *message, *file, *line;
#if PHP_VERSION_ID >= 70000
	zval rv;
#endif
	zend_class_entry *default_ce;

	if (APM_G(event_enabled)) {
		if (!exception) {
			return;
		}

		default_ce = zend_exception_get_default(TSRMLS_C);

#if PHP_VERSION_ID >= 70000
		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0, &rv);
		file = zend_read_property(default_ce, exception, "file", sizeof("file")-1, 0, &rv);
		line = zend_read_property(default_ce, exception, "line", sizeof("line")-1, 0, &rv);
#else
		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0 TSRMLS_CC);
		file = zend_read_property(default_ce, exception, "file", sizeof("file")-1, 0 TSRMLS_CC);
		line = zend_read_property(default_ce, exception, "line", sizeof("line")-1, 0 TSRMLS_CC);
#endif

		process_event(APM_EVENT_EXCEPTION, E_EXCEPTION, Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message) TSRMLS_CC);
	}
}

/* Insert an event in the backend */
static void process_event(int event_type, int type, char * error_filename, uint error_lineno, char * msg TSRMLS_DC)
{
	smart_str trace_str = {0};
	apm_driver_entry * driver_entry;

	if (APM_G(store_stacktrace)) {
		append_backtrace(&trace_str TSRMLS_CC);
		smart_str_0(&trace_str);
	}

	//mh print backtrace
	//APM_RECORD_EVENTS(error_filename);
	//APM_RECORD_EVENTS(msg);
	//char *record_buf = emalloc(1000);
	time_t timep;
	time(&timep);

#if PHP_VERSION_ID >= 70000
	//APM_RECORD_EVENTS(trace_str.s->val);
#else
	//APM_RECORD_EVENTS(trace_str.c);
#endif
	// delete record file
	// sprintf(
	// 		record_buf,
	// 		"%d + %s + %s + %s\n",
	// 		timep,error_filename, msg,
	// 	#if PHP_VERSION_ID >= 70000
	// 		trace_str.s->val
	// 	#else
	// 		trace_str.c
	// 	#endif

	// 		);

	// APM_RD(host_found) ? host_esc : "",
	// APM_RD(cookies_found) ? cookies_esc : "",
	// APM_RD(post_vars_found) ? post_vars_esc : "",
	// APM_RD(referer_found) ? referer_esc : "",
	// APM_RD(method_found) ? method_esc : "",
	// APM_RD(status_found) ? status_esc : "");

	// APM_RECORD_EVENTS(record_buf);
	// //efree(record_time_buf);
	// efree(record_buf);

	// record for file

	driver_entry = APM_G(drivers);
	APM_DEBUG("Direct processing process_event loop begin\n");
	while ((driver_entry = driver_entry->next) != NULL) {
		if (driver_entry->driver.want_event(event_type, type, msg TSRMLS_CC)) {
			driver_entry->driver.process_event(
				type,
				error_filename,
				error_lineno,
				msg,
#if PHP_VERSION_ID >= 70000
				(APM_G(store_stacktrace) && trace_str.s && trace_str.s->val) ? trace_str.s->val : ""
#else
				(APM_G(store_stacktrace) && trace_str.c) ? trace_str.c : ""
#endif
				TSRMLS_CC
			);
		}
	}
	APM_DEBUG("Direct processing process_event loop end\n");

	smart_str_free(&trace_str);
}

#if PHP_VERSION_ID >= 70000
#define REGISTER_INFO(name, dest, type) \
	if ((APM_RD(dest) = zend_hash_str_find(Z_ARRVAL_P(tmp), name, sizeof(name))) && (Z_TYPE_P(APM_RD(dest)) == (type))) { \
		APM_RD(dest##_found) = 1; \
	}
#else
#define REGISTER_INFO(name, dest, type) \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), name, sizeof(name), (void**)&APM_RD(dest)) == SUCCESS) && (Z_TYPE_PP(APM_RD(dest)) == (type))) { \
		APM_RD(dest##_found) = 1; \
	}
#endif

#if PHP_VERSION_ID >= 70000
#define FETCH_HTTP_GLOBALS(name) (tmp = &PG(http_globals)[TRACK_VARS_##name])
#else
#define FETCH_HTTP_GLOBALS(name) (tmp = PG(http_globals)[TRACK_VARS_##name])
#endif

void extract_data()
{
	zval *tmp;

	APM_DEBUG("Extracting data\n");

	if (APM_RD(initialized)) {
		APM_DEBUG("Data already initialized\n");
		return;
	}

	APM_RD(initialized) = 1;

	zend_is_auto_global_compat("_SERVER");
	if (FETCH_HTTP_GLOBALS(SERVER)) {
		REGISTER_INFO("REQUEST_URI", uri, IS_STRING);
		REGISTER_INFO("HTTP_HOST", host, IS_STRING);
		REGISTER_INFO("HTTP_REFERER", referer, IS_STRING);
		REGISTER_INFO("REQUEST_TIME", ts, IS_LONG);
		REGISTER_INFO("SCRIPT_FILENAME", script, IS_STRING);
		REGISTER_INFO("REQUEST_METHOD", method, IS_STRING);

		// add res status
		REGISTER_INFO("REDIRECT_STATUS", status, IS_STRING);

		if (APM_G(store_ip)) {
			REGISTER_INFO("REMOTE_ADDR", ip, IS_STRING);
		}

		//do record file  stats

		//char *record_buf = emalloc(1000);

		time_t timep;
		time(&timep);

		// sprintf(
		// 	record_buf,
		// 	"%d, %s, %s, %s, %s, %s, %f, %f, %f, %ld\n",
		// 	timep,
		// 	APM_RD_STRVAL(uri),
		// 	APM_RD_STRVAL(host),
		// 	APM_RD_STRVAL(ip),
		// 	APM_RD_STRVAL(method),
		// 	APM_RD_STRVAL(status),
		// 	APM_G(duration)/1000.0,

		// 	USEC_TO_SEC(APM_G(user_cpu)),
		// 	USEC_TO_SEC(APM_G(sys_cpu)),
		// 	APM_G(mem_peak_usage));

			// APM_RD(host_found) ? host_esc : "",
			// APM_RD(cookies_found) ? cookies_esc : "",
			// APM_RD(post_vars_found) ? post_vars_esc : "",
			// APM_RD(referer_found) ? referer_esc : "",
			// APM_RD(method_found) ? method_esc : "",
			// APM_RD(status_found) ? status_esc : "");

		//APM_RECORD_STATS(record_buf);
		//efree(record_time_buf);
		//efree(record_buf);

		//end do record file

	}
	if (APM_G(store_cookies)) {
		APM_DEBUG("need to store_cookies\n");
		zend_is_auto_global_compat("_COOKIE");
		if (FETCH_HTTP_GLOBALS(COOKIE)) {
			if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) {
				APM_G(buffer) = &APM_RD(cookies);
				zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC);
				APM_RD(cookies_found) = 1;
			}
		}
	}
	if (APM_G(store_post)) {
		APM_DEBUG("need to store_post\n");
		zend_is_auto_global_compat("_POST");
		if (FETCH_HTTP_GLOBALS(POST)) {
			if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) {
				APM_G(buffer) = &APM_RD(post_vars);
				zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC);
				APM_RD(post_vars_found) = 1;
			}
		}
	}
}

void do_file_record_stats() {
	extract_data();
}
void do_file_record_events() {

}

// mh add for trace
static void pt_frame_build(pt_frame_t *frame, zend_bool internal, unsigned char type, zend_execute_data *ex, zend_op_array *op_array TSRMLS_DC)
{
    int i;
    zval **args;
    zend_function *zf;

    /* init */
    memset(frame, 0, sizeof(pt_frame_t));

#if PHP_VERSION_ID < 50500
    if (internal || ex) {
        op_array = ex->op_array;
        zf = ex->function_state.function;
    } else {
        zf = (zend_function *) op_array;
    }
#else
    zf = ex->function_state.function;
#endif

    /* types, level */
    frame->type = type;
    frame->functype = internal ? PT_FUNC_INTERNAL : 0x00;
    frame->level = PTG(level);

    /* args init */
    args = NULL;
    frame->arg_count = 0;
    frame->args = NULL;

    /* names */
    if (zf->common.function_name) {
        /* functype, class name */
        if (ex && ex->object) {
            frame->functype |= PT_FUNC_MEMBER;
            /**
             * User care about which method is called exactly, so use
             * zf->common.scope->name instead of ex->object->name.
             */
            if (zf->common.scope) {
                frame->class = sdsnew(zf->common.scope->name);
            } else {
                /* TODO zend uses zend_get_object_classname() in
                 * debug_print_backtrace() */
                php_error(E_WARNING, "Trace catch a entry with ex->object but without zf->common.scope");
            }
        } else if (zf->common.scope) {
            frame->functype |= PT_FUNC_STATIC;
            frame->class = sdsnew(zf->common.scope->name);
        } else {
            frame->functype |= PT_FUNC_NORMAL;
        }

        /* function name */
        if (strcmp(zf->common.function_name, "{closure}") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{closure:%s:%d-%d}", zf->op_array.filename, zf->op_array.line_start, zf->op_array.line_end);
        } else if (strcmp(zf->common.function_name, "__lambda_func") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{lambda:%s}", zf->op_array.filename);
#if PHP_VERSION_ID >= 50414
        } else if (zf->common.scope && zf->common.scope->trait_aliases) {
            /* Use trait alias instead real function name.
             * There is also a bug "#64239 Debug backtrace changed behavior
             * since 5.4.10 or 5.4.11" about this
             * https://bugs.php.net/bug.php?id=64239.*/
            frame->function = sdsnew(zend_resolve_method_name(ex->object ? Z_OBJCE_P(ex->object) : zf->common.scope, zf));
#endif
        } else {
            frame->function = sdsnew(zf->common.function_name);
        }

        /* args */
#if PHP_VERSION_ID < 50300
        /* TODO support fetching arguments in backtrace */
        if (EG(argument_stack).top >= 2) {
            frame->arg_count = (int)(zend_uintptr_t) *(EG(argument_stack).top_element - 2);
            args = (zval **)(EG(argument_stack).top_element - 2 - frame->arg_count);
        }
#else
        if (ex && ex->function_state.arguments) {
            frame->arg_count = (int)(zend_uintptr_t) *(ex->function_state.arguments);
            args = (zval **)(ex->function_state.arguments - frame->arg_count);
        }
#endif
        if (frame->arg_count > 0) {
            frame->args = calloc(frame->arg_count, sizeof(sds));
            for (i = 0; i < frame->arg_count; i++) {
                frame->args[i] = pt_repr_zval(args[i], 32 TSRMLS_CC);
            }
        }
    } else {
        int add_filename = 1;
        long ev = 0;

#if ZEND_EXTENSION_API_NO < 220100525
        if (ex) {
            ev = ex->opline->op2.u.constant.value.lval;
        } else if (op_array && op_array->opcodes) {
            ev = op_array->opcodes->op2.u.constant.value.lval;
        }
#else
        if (ex) {
            ev = ex->opline->extended_value;
        } else if (op_array && op_array->opcodes) {
            ev = op_array->opcodes->extended_value;
        }
#endif

        /* special user function */
        switch (ev) {
            case ZEND_INCLUDE_ONCE:
                frame->functype |= PT_FUNC_INCLUDE_ONCE;
                frame->function = "include_once";
                break;
            case ZEND_REQUIRE_ONCE:
                frame->functype |= PT_FUNC_REQUIRE_ONCE;
                frame->function = "require_once";
                break;
            case ZEND_INCLUDE:
                frame->functype |= PT_FUNC_INCLUDE;
                frame->function = "include";
                break;
            case ZEND_REQUIRE:
                frame->functype |= PT_FUNC_REQUIRE;
                frame->function = "require";
                break;
            case ZEND_EVAL:
                frame->functype |= PT_FUNC_EVAL;
                frame->function = "{eval}"; /* TODO add eval code */
                add_filename = 0;
                break;
            default:
                /* should be function main */
                frame->functype |= PT_FUNC_NORMAL;
                frame->function = "{main}";
                add_filename = 0;
                break;
        }
        frame->function = sdsnew(frame->function);
        if (add_filename) {
            frame->arg_count = 1;
            frame->args = calloc(frame->arg_count, sizeof(sds));
            frame->args[0] = sdscatrepr(sdsempty(), zf->op_array.filename, strlen(zf->op_array.filename));
        }
    }

    /* lineno */
    if (ex && ex->opline) {
        frame->lineno = ex->opline->lineno;
    } else if (ex && ex->prev_execute_data && ex->prev_execute_data->opline) {
        frame->lineno = ex->prev_execute_data->opline->lineno; /* try using prev */
    } else if (op_array && op_array->opcodes) {
        frame->lineno = op_array->opcodes->lineno;
    /**
     * TODO Shall we use definition lineno if entry lineno is null
     * } else if (ex != EG(current_execute_data) && EG(current_execute_data)->opline) {
     *     frame->lineno = EG(current_execute_data)->opline->lineno; [> try using current <]
     */
    } else {
        frame->lineno = 0;
    }

    /* filename */
    if (ex && ex->op_array) {
        frame->filename = sdsnew(ex->op_array->filename);
    } else if (ex && ex->prev_execute_data && ex->prev_execute_data->op_array) {
        frame->filename = sdsnew(ex->prev_execute_data->op_array->filename); /* try using prev */
    } else if (op_array) {
        frame->filename = sdsnew(op_array->filename);
    /**
     * } else if (ex != EG(current_execute_data) && EG(current_execute_data)->op_array) {
     *     frame->filename = sdsnew(EG(current_execute_data)->op_array->filename); [> try using current <]
     */
    } else {
        frame->filename = NULL;
    }
}

static void pt_frame_destroy(pt_frame_t *frame TSRMLS_DC)
{
    int i;

    sdsfree(frame->filename);
    sdsfree(frame->class);
    sdsfree(frame->function);
    if (frame->args && frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            sdsfree(frame->args[i]);
        }
        free(frame->args);
    }
    sdsfree(frame->retval);
}

static void pt_frame_set_retval(pt_frame_t *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC)
{
    zval *retval = NULL;

    if (internal) {
        /* Ensure there is no exception occurs before fetching return value.
         * opline would be replaced by the Exception's opline if exception was
         * thrown which processed in function zend_throw_exception_internal().
         * It may cause a SEGMENTATION FAULT if we get the return value from a
         * exception opline. */
#if PHP_VERSION_ID >= 50500
        if (fci != NULL) {
            retval = *fci->retval_ptr_ptr;
        } else if (ex->opline && !EG(exception)) {
            retval = EX_TMP_VAR(ex, ex->opline->result.var)->var.ptr;
        }
#else
        if (ex->opline && !EG(exception)) {
#if PHP_VERSION_ID < 50400
            retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.u.var))->var.ptr;
#else
            retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.var))->var.ptr;
#endif
        }
#endif
    } else if (*EG(return_value_ptr_ptr)) {
        retval = *EG(return_value_ptr_ptr);
    }

    if (retval) {
        frame->retval = pt_repr_zval(retval, 32 TSRMLS_CC);
    }
}

// static int pt_frame_send(pt_frame_t *frame TSRMLS_DC)
// {
//     size_t len;
//     pt_comm_message_t *msg;

//     len = pt_type_len_frame(frame);
//     if ((msg = pt_comm_swrite_begin(&PTG(comm), len)) == NULL) {
//         php_error(E_WARNING, "Trace comm-module write begin failed, tried to allocate %ld bytes", len);
//         return -1;
//     }
//     pt_type_pack_frame(frame, msg->data);
//     pt_comm_swrite_end(&PTG(comm), PT_MSG_RET, msg);

//     return 0;
// }

static void pt_frame_display(pt_frame_t *frame TSRMLS_DC, zend_bool indent, const char *format, ...)
{
    int i, has_bracket = 1;
    size_t len;
    char *buf;
    va_list ap;

    // mh add
	char *record_buf = emalloc(1024);

	// APM_G(whole_trace_str) = emalloc(1024*16);
	//memset( APM_G(whole_trace_str), '\0', sizeof(APM_G(whole_trace_str)) );

    //mh add end

    /* indent */
    if (indent) {
        //zend_printf("%*s", (frame->level - 1) * 4, "");
        sprintf(record_buf, "%*s", (frame->level - 1) * 4, "");
        APM_RECORD_TRACE(record_buf);
        //strcat(APM_G(whole_trace_str), record_buf);
    }

    /* format */
    if (format) {
        va_start(ap, format);
        len = vspprintf(&buf, 0, format, ap); /* implementation of zend_vspprintf() */
        //mh delete display in user screen
        //zend_write(buf, len);
        efree(buf);
        va_end(ap);
    }

    /* frame */
    if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL ||
            frame->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        //zend_printf("%s(", frame->function);
        // add for trace
        sprintf(record_buf, "%s(", frame->function);
        APM_RECORD_TRACE(record_buf);
        strcat(APM_G(whole_trace_str), record_buf);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        //zend_printf("%s->%s(", frame->class, frame->function);
        sprintf(record_buf, "%s->%s(", frame->class, frame->function);
        APM_RECORD_TRACE(record_buf);
        strcat(APM_G(whole_trace_str), record_buf);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        //zend_printf("%s::%s(", frame->class, frame->function);
        sprintf(record_buf, "%s::%s(", frame->class, frame->function);
        APM_RECORD_TRACE(record_buf);
        strcat(APM_G(whole_trace_str), record_buf);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_EVAL) {
        //zend_printf("%s", frame->function);
        sprintf(record_buf, "%s", frame->function);
        APM_RECORD_TRACE(record_buf);
        strcat(APM_G(whole_trace_str), record_buf);
        has_bracket = 0;
    } else {
        //zend_printf("unknown");
        sprintf(record_buf, "%s", "unknown");
        APM_RECORD_TRACE(record_buf);
        strcat(APM_G(whole_trace_str), record_buf);
        has_bracket = 0;
    }

    /* arguments */
    if (frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            //zend_printf("%s", frame->args[i]);
        	sprintf(record_buf, "%s", frame->args[i]);
        	APM_RECORD_TRACE(record_buf);
        	strcat(APM_G(whole_trace_str), record_buf);

            if (i < frame->arg_count - 1) {
                //zend_printf(", ");
        		sprintf(record_buf, "%s", ", ");
        		APM_RECORD_TRACE(record_buf);
        		strcat(APM_G(whole_trace_str), record_buf);
            }
        }
    }
    if (has_bracket) {
        //zend_printf(")");
       	sprintf(record_buf, "%s", ")");
		APM_RECORD_TRACE(record_buf);
		strcat(APM_G(whole_trace_str), record_buf);
    }

    /* return value */
    // mh modify
  	//   if (frame->type == PT_FRAME_EXIT && frame->retval) {
  	//       zend_printf(" = %s", frame->retval);
  	//      	sprintf(record_buf, " = %s", frame->retval);
		// APM_RECORD_TRACE(record_buf);
		// strcat(APM_G(whole_trace_str), record_buf);
  	//   }

    /* TODO output relative filepath */
 	//    zend_printf(" called at [%s:%d]", frame->filename, frame->lineno);
 	//   	sprintf(record_buf, " called at [%s:%d]", frame->filename, frame->lineno);
	// APM_RECORD_TRACE(record_buf);
	// strcat(APM_G(whole_trace_str), record_buf);

    if (frame->type == PT_FRAME_EXIT) {
        //zend_printf(" wt: %.3f ct: %.3f\n",
                //((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0),
                //((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0));
       	sprintf(record_buf, " wt:%.3f ct:%.3f total:%.3f+",
                ((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0),
                ((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0),
       			((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0)+
                ((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0));
		APM_RECORD_TRACE(record_buf);
		strcat(APM_G(whole_trace_str), record_buf);
    } else {
        //zend_printf("\n");
        APM_RECORD_TRACE("\n");
        strcat(APM_G(whole_trace_str), "+");
    }

    //mh add
    efree(record_buf);
}

/**
 * Trace Manipulation of Status
 * --------------------
 */
static void pt_status_build(pt_status_t *status, zend_bool internal, zend_execute_data *ex TSRMLS_DC)
{
    int i;
    zend_execute_data *ex_ori = ex;

    /* init */
    memset(status, 0, sizeof(pt_status_t));

    /* common */
    status->php_version = PHP_VERSION;
    status->sapi_name = sapi_module.name;

    /* profile */
    status->mem = zend_memory_usage(0 TSRMLS_CC);
    status->mempeak = zend_memory_peak_usage(0 TSRMLS_CC);
    status->mem_real = zend_memory_usage(1 TSRMLS_CC);
    status->mempeak_real = zend_memory_peak_usage(1 TSRMLS_CC);

    /* request */
    status->request_method = (char *) SG(request_info).request_method;
    status->request_uri = SG(request_info).request_uri;
    status->request_query = SG(request_info).query_string;
    status->request_time = SG(global_request_time);
    status->request_script = SG(request_info).path_translated;
    status->proto_num = SG(request_info).proto_num;

    /* arguments */
    status->argc = SG(request_info).argc;
    status->argv = SG(request_info).argv;

    /* frame */
    for (i = 0; ex; ex = ex->prev_execute_data, i++) ; /* calculate stack depth */
    status->frame_count = i;
    if (status->frame_count) {
        status->frames = calloc(status->frame_count, sizeof(pt_frame_t));
        for (i = 0, ex = ex_ori; i < status->frame_count && ex; i++, ex = ex->prev_execute_data) {
            pt_frame_build(status->frames + i, internal, PT_FRAME_STACK, ex, NULL TSRMLS_CC);
            status->frames[i].level = 1;
        }
    } else {
        status->frames = NULL;
    }
}

static void pt_status_destroy(pt_status_t *status TSRMLS_DC)
{
    int i;

    if (status->frames && status->frame_count) {
        for (i = 0; i < status->frame_count; i++) {
            pt_frame_destroy(status->frames + i TSRMLS_CC);
        }
        free(status->frames);
    }
}

static void pt_status_display(pt_status_t *status TSRMLS_DC)
{
    int i;

    zend_printf("------------------------------- Status --------------------------------\n");
    zend_printf("PHP Version:       %s\n", status->php_version);
    zend_printf("SAPI:              %s\n", status->sapi_name);

    zend_printf("memory:            %.2fm\n", status->mem / 1048576.0);
    zend_printf("memory peak:       %.2fm\n", status->mempeak / 1048576.0);
    zend_printf("real-memory:       %.2fm\n", status->mem_real / 1048576.0);
    zend_printf("real-memory peak   %.2fm\n", status->mempeak_real / 1048576.0);

    zend_printf("------------------------------- Request -------------------------------\n");
    if (status->request_method) {
    zend_printf("request method:    %s\n", status->request_method);
    }
    zend_printf("request time:      %f\n", status->request_time);
    if (status->request_uri) {
    zend_printf("request uri:       %s\n", status->request_uri);
    }
    if (status->request_query) {
    zend_printf("request query:     %s\n", status->request_query);
    }
    if (status->request_script) {
    zend_printf("request script:    %s\n", status->request_script);
    }
    zend_printf("proto_num:         %d\n", status->proto_num);

    if (status->argc) {
        zend_printf("------------------------------ Arguments ------------------------------\n");
        for (i = 0; i < status->argc; i++) {
            zend_printf("$%-10d        %s\n", i, status->argv[i]);
        }
    }

    if (status->frame_count) {
        zend_printf("------------------------------ Backtrace ------------------------------\n");
        for (i = 0; i < status->frame_count; i++) {
            pt_frame_display(status->frames + i TSRMLS_CC, 0, "#%-3d", i);
        }
    }
}

// static int pt_status_send(pt_status_t *status TSRMLS_DC)
// {
//     size_t len;
//     pt_comm_message_t *msg;

//     len = pt_type_len_status(status);
//     if ((msg = pt_comm_swrite_begin(&PTG(comm), len)) == NULL) {
//         php_error(E_WARNING, "Trace comm-module write begin failed, tried to allocate %ld bytes", len);
//         return -1;
//     }
//     pt_type_pack_status(status, msg->data);
//     pt_comm_swrite_end(&PTG(comm), PT_MSG_RET, msg);

//     return 0;
// }

/**
 * Trace Misc Function
 * --------------------
 */
static sds pt_repr_zval(zval *zv, int limit TSRMLS_DC)
{
    int tlen = 0;
    char buf[256], *tstr = NULL;
    sds result;

    /* php_var_export_ex is a good example */
    switch (Z_TYPE_P(zv)) {
        case IS_BOOL:
            if (Z_LVAL_P(zv)) {
                return sdsnew("true");
            } else {
                return sdsnew("false");
            }
        case IS_NULL:
            return sdsnew("NULL");
        case IS_LONG:
            snprintf(buf, sizeof(buf), "%ld", Z_LVAL_P(zv));
            return sdsnew(buf);
        case IS_DOUBLE:
            snprintf(buf, sizeof(buf), "%.*G", (int) EG(precision), Z_DVAL_P(zv));
            return sdsnew(buf);
        case IS_STRING:
            tlen = (limit <= 0 || Z_STRLEN_P(zv) < limit) ? Z_STRLEN_P(zv) : limit;
            result = sdscatrepr(sdsempty(), Z_STRVAL_P(zv), tlen);
            if (limit > 0 && Z_STRLEN_P(zv) > limit) {
                result = sdscat(result, "...");
            }
            return result;
        case IS_ARRAY:
            /* TODO more info */
            return sdscatprintf(sdsempty(), "array(%d)", zend_hash_num_elements(Z_ARRVAL_P(zv)));
        case IS_OBJECT:
            if (Z_OBJ_HANDLER(*zv, get_class_name)) {
                Z_OBJ_HANDLER(*zv, get_class_name)(zv, (const char **) &tstr, (zend_uint *) &tlen, 0 TSRMLS_CC);
                result = sdscatprintf(sdsempty(), "object(%s)#%d", tstr, Z_OBJ_HANDLE_P(zv));
                efree(tstr);
            } else {
                result = sdscatprintf(sdsempty(), "object(unknown)#%d", Z_OBJ_HANDLE_P(zv));
            }
            return result;
        case IS_RESOURCE:
            tstr = (char *) zend_rsrc_list_get_rsrc_type(Z_LVAL_P(zv) TSRMLS_CC);
            return sdscatprintf(sdsempty(), "resource(%s)#%ld", tstr ? tstr : "Unknown", Z_LVAL_P(zv));
        default:
            return sdsnew("{unknown}");
    }
}

static void pt_set_inactive(TSRMLS_D)
{
    //PTD("Ctrl set inactive");
    //CTRL_SET_INACTIVE();

    /* toggle trace off, close comm-module */
    PTG(dotrace) &= ~TRACE_TO_TOOL;
    // if (PTG(comm).active) {
    //     //PTD("Comm socket close");
    //     pt_comm_sclose(&PTG(comm), 1);
    // }
}

/**
 * Trace Executor Replacement
 * --------------------
 */
#if PHP_VERSION_ID < 50500
ZEND_API void pt_execute_core(int internal, zend_execute_data *execute_data, zend_op_array *op_array, int rvu TSRMLS_DC)
{
    zend_fcall_info *fci = NULL;
#else
ZEND_API void pt_execute_core(int internal, zend_execute_data *execute_data, zend_fcall_info *fci, int rvu TSRMLS_DC)
{
    zend_op_array *op_array = NULL;
#endif
    long dotrace;
    zend_bool dobailout = 0;
    zend_execute_data *ex_entry = execute_data;
    zval *retval = NULL;
    pt_comm_message_t *msg;
    pt_frame_t frame;

#if PHP_VERSION_ID >= 50500
    /* Why has a ex_entry ?
     * In PHP 5.5 and after, execute_data is the data going to be executed, not
     * the entry point, so we switch to previous data. The internal function is
     * a exception because it's no need to execute by op_array. */
    if (!internal && execute_data->prev_execute_data) {
        ex_entry = execute_data->prev_execute_data;
    }
#endif
    //zend_printf("############1#####\n");
    /* Check ctrl module */
    //if (CTRL_IS_ACTIVE()) {
        // /* Open comm socket */
        // if (!PTG(comm).active) {
        //     //PTD("Comm socket %s open", PTG(comm_file));
        //     if (pt_comm_sopen(&PTG(comm), PTG(comm_file), 0) == -1) {
        //         php_error(E_WARNING, "Trace comm-module %s open failed", PTG(comm_file));
        //         pt_set_inactive(TSRMLS_C);
        //         goto exec;
        //     }
        //     PING_UPDATE();
        // }

        // /* Handle message */
        // while (1) {
        //     switch (pt_comm_sread(&PTG(comm), &msg, 1)) {
        //         case PT_MSG_INVALID:
        //             //PTD("msg type: invalid");
        //             pt_set_inactive(TSRMLS_C);
        //             goto exec;

        //         case PT_MSG_EMPTY:
        //             //PTD("msg type: empty");
        //             if (PING_TIMEOUT()) {
        //                 //PTD("Ping timeout");
        //                 pt_set_inactive(TSRMLS_C);
        //             }
        //             goto exec;

        //         case PT_MSG_DO_TRACE:
        //             //PTD("msg type: do_trace");
        //             PTG(dotrace) |= TRACE_TO_TOOL;
        //             break;

        //         case PT_MSG_DO_STATUS:
        //             //PTD("msg type: do_status");
        //             //pt_status_t status;
        //             //pt_status_build(&status, internal, ex_entry TSRMLS_CC);
        //             //pt_status_send(&status TSRMLS_CC);
        //             //pt_status_destroy(&status TSRMLS_CC);
        //             break;

        //         case PT_MSG_DO_PING:
        //             //PTD("msg type: do_ping");
        //             PING_UPDATE();
        //             break;

        //         default:
        //             php_error(E_NOTICE, "Trace unknown message received with type 0x%x", msg->type);
        //             break;
        //     }
        // }
    //} else if (PTG(comm).active) { /* comm-module still open */
    //   pt_set_inactive(TSRMLS_C);
    //}

exec:
    /* Assigning to a LOCAL VARIABLE at begining to prevent value changed
     * during executing. And whether send frame mesage back is controlled by
     * GLOBAL VALUE and LOCAL VALUE both because comm-module may be closed in
     * recursion and sending on exit point will be affected. */
    dotrace = PTG(dotrace);

    PTG(level)++;

    //if (dotrace) {
    if (dotrace) {
        pt_frame_build(&frame, internal, PT_FRAME_ENTRY, ex_entry, op_array TSRMLS_CC);

        /* Register reture value ptr */
        if (!internal && EG(return_value_ptr_ptr) == NULL) {
            EG(return_value_ptr_ptr) = &retval;
        }

        PROFILING_SET(frame.entry);

        /* Send frame message */
        //if (dotrace & TRACE_TO_TOOL) {
        //    pt_frame_send(&frame TSRMLS_CC);
        //}
        //if (dotrace & TRACE_TO_OUTPUT) {
        if (dotrace) {
            pt_frame_display(&frame TSRMLS_CC, 1, "> ");
        }

    }

    /* Call original under zend_try. baitout will be called when exit(), error
     * occurs, exception thrown and etc, so we have to catch it and free our
     * resources. */
    zend_try {
#if PHP_VERSION_ID < 50500
        if (internal) {
            if (pt_ori_execute_internal) {
                pt_ori_execute_internal(execute_data, rvu TSRMLS_CC);
            } else {
                execute_internal(execute_data, rvu TSRMLS_CC);
            }
        } else {
            pt_ori_execute(op_array TSRMLS_CC);
        }
#else
        if (internal) {
            if (pt_ori_execute_internal) {
                pt_ori_execute_internal(execute_data, fci, rvu TSRMLS_CC);
            } else {
                execute_internal(execute_data, fci, rvu TSRMLS_CC);
            }
        } else {
            pt_ori_execute_ex(execute_data TSRMLS_CC);
        }
#endif
    } zend_catch {
        dobailout = 1;
        /* call zend_bailout() at the end of this function, we still want to
         * send message. */
    } zend_end_try();

    //if (dotrace) {
    if (dotrace) {
        PROFILING_SET(frame.exit);

        if (!dobailout) {
            pt_frame_set_retval(&frame, internal, execute_data, fci TSRMLS_CC);
        }
        frame.type = PT_FRAME_EXIT;

        // /* Send frame message */
        // if (PTG(dotrace) & TRACE_TO_TOOL & dotrace) {
        //     pt_frame_send(&frame TSRMLS_CC);
        // }
        //if (PTG(dotrace) & TRACE_TO_OUTPUT & dotrace) {
        if (1) {
            pt_frame_display(&frame TSRMLS_CC, 1, "< ");

        }

        /* Free reture value */
        if (!internal && retval != NULL) {
            zval_ptr_dtor(&retval);
            EG(return_value_ptr_ptr) = NULL;
        }

        pt_frame_destroy(&frame TSRMLS_CC);
    }

    PTG(level)--;

    if (dobailout) {
        zend_bailout();
    }
}

#if PHP_VERSION_ID < 50500
ZEND_API void pt_execute(zend_op_array *op_array TSRMLS_DC)
{
    pt_execute_core(0, EG(current_execute_data), op_array, 0 TSRMLS_CC);
}

ZEND_API void pt_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC)
{
    pt_execute_core(1, execute_data, NULL, return_value_used TSRMLS_CC);
}
#else
ZEND_API void pt_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    pt_execute_core(0, execute_data, NULL, 0 TSRMLS_CC);
}

ZEND_API void pt_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    pt_execute_core(1, execute_data, fci, return_value_used TSRMLS_CC);
}
#endif
//mh add end
