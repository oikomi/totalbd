/*
 * php_apm.h
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#ifndef PHP_APM_H
#define PHP_APM_H

#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_errors.h"

#if PHP_VERSION_ID >= 70000
#include "zend_smart_str.h"
#else
#include "ext/standard/php_smart_str.h"
#endif

#ifndef E_EXCEPTION
#define E_EXCEPTION (1<<15L)
#endif

#ifdef APM_DRIVER_MYSQL
#include <mysql/mysql.h>
#endif

#ifdef PHP_WIN32
#define PHP_APM_API __declspec(dllexport)
#else
#define PHP_APM_API
#endif

#include "TSRM.h"

 //mh add
#include "const.h"
#include "common/trace_time.h"
#include "common/trace_type.h"
#include "common/trace_ctrl.h"
#include "common/trace_comm.h"
#include "common/sds/sds.h"

#include "zend_extensions.h"
#include "SAPI.h"

#define HAVE_GETRUSAGE 1

#define APM_E_ALL (E_ALL | E_STRICT)

#define APM_EVENT_ERROR 1
#define APM_EVENT_EXCEPTION 2

#define TRACE_STACK_SIZE  1024*64

#define PROCESS_EVENT_ARGS int type, char * error_filename, uint error_lineno, char * msg, char * trace  TSRMLS_DC

typedef struct apm_event {
	int event_type;
	int type;
	char * error_filename;
	uint error_lineno;
	char * msg;
	char * trace;
} apm_event;

typedef struct apm_event_entry {
	apm_event event;
	struct apm_event_entry *next;
} apm_event_entry;

typedef struct apm_driver {
	void (* process_event)(PROCESS_EVENT_ARGS);
	void (* process_stats)(TSRMLS_D);
	int (* minit)(int TSRMLS_DC);
	int (* rinit)(TSRMLS_D);
	int (* mshutdown)(SHUTDOWN_FUNC_ARGS);
	int (* rshutdown)(TSRMLS_D);
	zend_bool (* is_enabled)(TSRMLS_D);
	zend_bool (* want_event)(int, int, char * TSRMLS_DC);
	zend_bool (* want_stats)(TSRMLS_D);
	int (* error_reporting)(TSRMLS_D);
	zend_bool is_request_created;
} apm_driver;

typedef struct apm_driver_entry {
	apm_driver driver;
	struct apm_driver_entry *next;
} apm_driver_entry;

#if PHP_VERSION_ID >= 70000
#define RD_DEF(var) zval *var; zend_bool var##_found;
#else
#define RD_DEF(var) zval **var; zend_bool var##_found;
#endif

typedef struct apm_request_data {
	RD_DEF(uri);
	RD_DEF(host);
	RD_DEF(ip);
	RD_DEF(referer);
	RD_DEF(ts);
	RD_DEF(script);
	RD_DEF(method);
	RD_DEF(status);

	zend_bool initialized, cookies_found, post_vars_found;
	smart_str cookies, post_vars;
} apm_request_data;

#ifdef ZTS
#define APM_GLOBAL(driver, v) TSRMG(apm_globals_id, zend_apm_globals *, driver##_##v)
#else
#define APM_GLOBAL(driver, v) (apm_globals.driver##_##v)
#endif

#if PHP_VERSION_ID >= 70000
#define apm_error_reporting_new_value (new_value && new_value->val) ? atoi(new_value->val)
#else
#define apm_error_reporting_new_value new_value ? atoi(new_value)
#endif

#define APM_DRIVER_CREATE(name) \
void apm_driver_##name##_process_event(PROCESS_EVENT_ARGS); \
void apm_driver_##name##_process_stats(TSRMLS_D); \
int apm_driver_##name##_minit(int TSRMLS_DC); \
int apm_driver_##name##_rinit(TSRMLS_D); \
int apm_driver_##name##_mshutdown(); \
int apm_driver_##name##_rshutdown(TSRMLS_D); \
PHP_INI_MH(OnUpdateAPM##name##ErrorReporting) \
{ \
	APM_GLOBAL(name, error_reporting) = (apm_error_reporting_new_value : APM_E_##name); \
	return SUCCESS; \
} \
zend_bool apm_driver_##name##_is_enabled(TSRMLS_D) \
{ \
	return APM_GLOBAL(name, enabled); \
} \
int apm_driver_##name##_error_reporting(TSRMLS_D) \
{ \
	return APM_GLOBAL(name, error_reporting); \
} \
zend_bool apm_driver_##name##_want_event(int event_type, int error_level, char *msg TSRMLS_DC) \
{ \
	return \
		APM_GLOBAL(name, enabled) \
		&& ( \
			(event_type == APM_EVENT_EXCEPTION && APM_GLOBAL(name, exception_mode) == 2) \
			|| \
			(event_type == APM_EVENT_ERROR && ((APM_GLOBAL(name, exception_mode) == 1) || (strncmp(msg, "Uncaught exception", 18) != 0)) && (error_level & APM_GLOBAL(name, error_reporting))) \
		) \
		&& ( \
			!APM_G(currently_silenced) || APM_GLOBAL(name, process_silenced_events) \
		) \
	; \
} \
zend_bool apm_driver_##name##_want_stats(TSRMLS_D) \
{ \
	return \
		APM_GLOBAL(name, enabled) \
		&& ( \
			APM_GLOBAL(name, stats_enabled)\
		) \
	; \
} \
apm_driver_entry * apm_driver_##name##_create() \
{ \
	apm_driver_entry * driver_entry; \
	driver_entry = (apm_driver_entry *) malloc(sizeof(apm_driver_entry)); \
	driver_entry->driver.process_event = apm_driver_##name##_process_event; \
	driver_entry->driver.minit = apm_driver_##name##_minit; \
	driver_entry->driver.rinit = apm_driver_##name##_rinit; \
	driver_entry->driver.mshutdown = apm_driver_##name##_mshutdown; \
	driver_entry->driver.rshutdown = apm_driver_##name##_rshutdown; \
	driver_entry->driver.process_stats = apm_driver_##name##_process_stats; \
	driver_entry->driver.is_enabled = apm_driver_##name##_is_enabled; \
	driver_entry->driver.error_reporting = apm_driver_##name##_error_reporting; \
	driver_entry->driver.want_event = apm_driver_##name##_want_event; \
	driver_entry->driver.want_stats = apm_driver_##name##_want_stats; \
	driver_entry->driver.is_request_created = 0; \
	driver_entry->next = NULL; \
	return driver_entry; \
}

PHP_MINIT_FUNCTION(apm);
PHP_MSHUTDOWN_FUNCTION(apm);
PHP_RINIT_FUNCTION(apm);
PHP_RSHUTDOWN_FUNCTION(apm);
PHP_MINFO_FUNCTION(apm);

#ifdef APM_RECORD_USE_FILE
// add file stats record
#define APM_INIT_FILE_STATS_RECORD APM_G(recordfilestats) = fopen(FILE_RECORD_STATS, "a+");
#define APM_RECORD_STATS(...) if (APM_G(recordfilestats)) { fprintf(APM_G(recordfilestats), __VA_ARGS__); fflush(APM_G(recordfilestats)); }
#define APM_SHUTDOWN_FILE_STATS_RECODE if (APM_G(recordfilestats)) { fclose(APM_G(recordfilestats)); APM_G(recordfilestats) = NULL; }
//end

// add file events record
#define APM_INIT_FILE_EVENTS_RECORD APM_G(recordfileevents) = fopen(FILE_RECORD_EVENTS, "a+");
#define APM_RECORD_EVENTS(...) if (APM_G(recordfileevents)) { fprintf(APM_G(recordfileevents), __VA_ARGS__); fflush(APM_G(recordfileevents)); }
#define APM_SHUTDOWN_FILE_EVENTS_RECODE if (APM_G(recordfileevents)) { fclose(APM_G(recordfileevents)); APM_G(recordfileevents) = NULL; }

//end

// add file trace record
#define APM_INIT_FILE_TRACE_RECORD APM_G(recordfiletrace) = fopen(FILE_RECORD_TRACE, "a+");
#define APM_RECORD_TRACE(...) if (APM_G(recordfiletrace)) { fprintf(APM_G(recordfiletrace), __VA_ARGS__); fflush(APM_G(recordfiletrace)); }
#define APM_SHUTDOWN_FILE_TRACE_RECODE if (APM_G(recordfiletrace)) { fclose(APM_G(recordfiletrace)); APM_G(recordfiletrace) = NULL; }
//end

#else

// add file stats record
#define APM_INIT_FILE_STATS_RECORD
#define APM_RECORD_STATS(...)
#define APM_SHUTDOWN_FILE_STATS_RECODE
//end

// add file events record
#define APM_INIT_FILE_EVENTS_RECORD
#define APM_RECORD_EVENTS(...)
#define APM_SHUTDOWN_FILE_EVENTS_RECODE
//end

// add file trace record
#define APM_INIT_FILE_TRACE_RECORD
#define APM_RECORD_TRACE(...)
#define APM_SHUTDOWN_FILE_TRACE_RECODE 
//end

#endif

#define APM_DEBUGFILE "/tmp/baidu.log"

#ifdef APM_DEBUGFILE
#define APM_INIT_DEBUG APM_G(debugfile) = fopen(APM_DEBUGFILE, "a+");
#define APM_DEBUG(...) if (APM_G(debugfile)) { fprintf(APM_G(debugfile), __VA_ARGS__); fflush(APM_G(debugfile)); }
#define APM_SHUTDOWN_DEBUG if (APM_G(debugfile)) { fclose(APM_G(debugfile)); APM_G(debugfile) = NULL; }
#else
#define APM_INIT_DEBUG
#define APM_DEBUG(...)
#define APM_SHUTDOWN_DEBUG
#endif

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm)
	/* Boolean controlling whether the extension is globally active or not */
	zend_bool enabled;
	/* Application identifier, helps identifying which application is being monitored */
	char      *application_id;
	/* Boolean controlling whether the event monitoring is active or not */
	zend_bool event_enabled;
	/* Boolean controlling whether the stacktrace should be generated or not */
	zend_bool store_stacktrace;
	/* Boolean controlling whether the ip should be generated or not */
	zend_bool store_ip;
	/* Boolean controlling whether the cookies should be generated or not */
	zend_bool store_cookies;
	/* Boolean controlling whether the POST variables should be generated or not */
	zend_bool store_post;
	/* Time (in ms) before a request is considered for stats */
	long      stats_duration_threshold;
	/* User CPU time usage (in ms) before a request is considered for stats */
	long      stats_user_cpu_threshold;
	/* System CPU time usage (in ms) before a request is considered for stats */
	long      stats_sys_cpu_threshold;
	/* Maximum recursion depth used when dumping a variable */
	long      dump_max_depth;
	/* Determines whether we're currently silenced */
	zend_bool currently_silenced;

	apm_driver_entry *drivers;
	smart_str *buffer;

	/* Structure used to store request data */
	apm_request_data request_data;

	float duration;

	long mem_peak_usage;
#ifdef HAVE_GETRUSAGE
	float user_cpu;

	float sys_cpu;
#endif

#ifdef APM_DEBUGFILE
	FILE * debugfile;
#endif

	// add record file
	FILE * recordfilestats;
	FILE * recordfileevents;
	FILE * recordfiletrace;

#ifdef APM_DRIVER_MYSQL
	/* Boolean controlling whether the driver is active or not */
	zend_bool mysql_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool mysql_stats_enabled;
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	long mysql_exception_mode;
	/* driver error reporting */
	int mysql_error_reporting;
	/* MySQL host */
	char *mysql_db_host;
	/* MySQL port */
	unsigned int mysql_db_port;
	/* MySQL user */
	char *mysql_db_user;
	/* MySQL password */
	char *mysql_db_pass;
	/* MySQL database */
	char *mysql_db_name;
	/* DB handle */
	MYSQL *mysql_event_db;
	/* Option to process silenced events */
	zend_bool mysql_process_silenced_events;

	/* Boolean to ensure request content is only inserted once */
	zend_bool mysql_is_request_created;
#endif

#ifdef APM_DRIVER_SOCKET
	/* Boolean controlling whether the driver is active or not */
	zend_bool socket_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool socket_stats_enabled;
	/* (unused for StatsD) */
	long socket_exception_mode;
	/* (unused for StatsD) */
	int socket_error_reporting;
	/* Option to process silenced events */
	zend_bool socket_process_silenced_events;
	/* socket path */
	char *socket_path;
	apm_event_entry *socket_events;
	apm_event_entry **socket_last_event;
#endif

	// mh add for phptrace

    pid_t                   pid;            /* process id */
    long                    level;          /* nesting level */

    long                    ping;           /* last ping time (second) */
    long                    idle_timeout;   /* idle timeout, for current - last ping */

    long                    dotrace;        /* flags of trace */

    char                    whole_trace_str[TRACE_STACK_SIZE];

    //char                    *data_dir;      /* data path, should be writable */

    //pt_ctrl_t               ctrl;           /* ctrl module */
    //char                    ctrl_file[256]; /* ctrl filename */

    //pt_comm_socket_t        comm;           /* comm module */
    //char                    comm_file[256]; /* comm filename */

	//mh add end
ZEND_END_MODULE_GLOBALS(apm)

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

#ifdef ZTS
#define PTG(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define PTG(v) (apm_globals.v)
#endif

#define APM_RD(data) APM_G(request_data).data

#if PHP_VERSION_ID >= 70000
# define APM_RD_STRVAL(var) Z_STRVAL_P(APM_RD(var))
# define APM_RD_SMART_STRVAL(var) APM_RD(var).s->val
#else
# define APM_RD_STRVAL(var) Z_STRVAL_PP(APM_RD(var))
# define APM_RD_SMART_STRVAL(var) APM_RD(var).c
#endif

#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

#if PHP_VERSION_ID >= 70000
# define zend_is_auto_global_compat(name) (zend_is_auto_global_str(ZEND_STRL((name))))
# define add_assoc_long_compat(array, key, value) add_assoc_long_ex((array), (key), (sizeof(key) - 1), (value));
#else
# define zend_is_auto_global_compat(name) (zend_is_auto_global(ZEND_STRL((name)) TSRMLS_CC))
# define add_assoc_long_compat(array, key, value) add_assoc_long_ex((array), (key), (sizeof(key)), (value));
#endif

int apm_write(const char *str,
#if PHP_VERSION_ID >= 70000
size_t
#else
uint
#endif
length);

#endif

void extract_data();

void do_file_record_stats();
void do_file_record_events();

//mh add

static void pt_frame_build(pt_frame_t *frame, zend_bool internal, unsigned char type, zend_execute_data *ex, zend_op_array *op_array TSRMLS_DC);
static void pt_frame_destroy(pt_frame_t *frame TSRMLS_DC);
static void pt_frame_display(pt_frame_t *frame TSRMLS_DC, zend_bool indent, const char *format, ...);
static int pt_frame_send(pt_frame_t *frame TSRMLS_DC);
static void pt_frame_set_retval(pt_frame_t *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC);

static void pt_status_build(pt_status_t *status, zend_bool internal, zend_execute_data *ex TSRMLS_DC);
static void pt_status_destroy(pt_status_t *status TSRMLS_DC);
static void pt_status_display(pt_status_t *status TSRMLS_DC);
static int pt_status_send(pt_status_t *status TSRMLS_DC);

static sds pt_repr_zval(zval *zv, int limit TSRMLS_DC);
static void pt_set_inactive(TSRMLS_D);

#if PHP_VERSION_ID < 50500
static void (*pt_ori_execute)(zend_op_array *op_array TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);
ZEND_API void pt_execute(zend_op_array *op_array TSRMLS_DC);
ZEND_API void pt_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC);
#else
static void (*pt_ori_execute_ex)(zend_execute_data *execute_data TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
ZEND_API void pt_execute_ex(zend_execute_data *execute_data TSRMLS_DC);
ZEND_API void pt_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
#endif
// mh add end
