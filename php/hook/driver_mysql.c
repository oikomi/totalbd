/*
 * driver_mysql.c
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#include <mysql/mysql.h>
#include "php_apm.h"
#include "php_ini.h"
#include "driver_mysql.h"

#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(apm);

APM_DRIVER_CREATE(mysql)

static void mysql_destroy(TSRMLS_D) {
	APM_DEBUG("[MySQL driver] Closing connection\n");
	mysql_close(APM_G(mysql_event_db));
	free(APM_G(mysql_event_db));
	APM_G(mysql_event_db) = NULL;
	mysql_library_end();
}

/* Returns the MYSQL instance (singleton) */
MYSQL * mysql_get_instance(TSRMLS_D) {
	my_bool reconnect = 1;
	if (APM_G(mysql_event_db) == NULL) {
		APM_DEBUG("[MySQL driver] mysql_event_db is null (mysql handler)... \n");
		mysql_library_init(0, NULL, NULL);
		APM_G(mysql_event_db) = malloc(sizeof(MYSQL));

		mysql_init(APM_G(mysql_event_db));

		mysql_options(APM_G(mysql_event_db), MYSQL_OPT_RECONNECT, &reconnect);
		APM_DEBUG("[MySQL driver] Connecting to server...");
		if (mysql_real_connect(APM_G(mysql_event_db), APM_G(mysql_db_host), APM_G(mysql_db_user), APM_G(mysql_db_pass), APM_G(mysql_db_name), APM_G(mysql_db_port), NULL, 0) == NULL) {
			APM_DEBUG("FAILED! Message: %s\n", mysql_error(APM_G(mysql_event_db)));

			mysql_destroy(TSRMLS_C);
			return NULL;
		}
		APM_DEBUG("OK\n");

		mysql_set_character_set(APM_G(mysql_event_db), "utf8");

		mysql_query(
			APM_G(mysql_event_db),
			"\
			CREATE TABLE IF NOT EXISTS request (\
			    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
			    application VARCHAR(255) NOT NULL,\
			    ts TIMESTAMP NOT NULL,\
			    script TEXT NOT NULL,\
			    uri TEXT NOT NULL,\
			    host TEXT NOT NULL,\
			    ip INTEGER UNSIGNED NOT NULL,\
			    cookies TEXT NOT NULL,\
			    post_vars TEXT NOT NULL,\
			    referer TEXT NOT NULL,\
			    method TEXT NOT NULL,\
			    status TEXT NOT NULL\
			)"
	);
		mysql_query(
			APM_G(mysql_event_db),
						"\
			CREATE TABLE IF NOT EXISTS event (\
			    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
			    request_id INTEGER UNSIGNED,\
			    ts TIMESTAMP NOT NULL,\
			    type SMALLINT UNSIGNED NOT NULL,\
			    file TEXT NOT NULL,\
			    line MEDIUMINT UNSIGNED NOT NULL,\
			    message TEXT NOT NULL,\
			    backtrace BLOB NOT NULL,\
			    KEY request (request_id)\
			)"
	);

		mysql_query(
			APM_G(mysql_event_db),
			"\
CREATE TABLE IF NOT EXISTS stats (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    request_id INTEGER UNSIGNED,\
    duration FLOAT UNSIGNED NOT NULL,\
    user_cpu FLOAT UNSIGNED NOT NULL,\
    sys_cpu FLOAT UNSIGNED NOT NULL,\
    mem_peak_usage INTEGER UNSIGNED NOT NULL,\
    KEY request (request_id)\
)"
		);
	}

	return APM_G(mysql_event_db);
}

/* Escape string request data */
#define APM_MYSQL_ESCAPE_STR(data) \
{ \
	if (APM_RD(data##_found)) { \
		data##_len = strlen(APM_RD_STRVAL(data)); \
		data##_esc = emalloc(data##_len * 2 + 1); \
		data##_len = mysql_real_escape_string(connection, data##_esc, APM_RD_STRVAL(data), data##_len); \
	} \
}

/* Escape smart_str request data */
#define APM_MYSQL_ESCAPE_SMART_STR(data) \
{ \
	if (APM_RD(data##_found)) { \
		data##_len = strlen(APM_RD_SMART_STRVAL(data)); \
		data##_esc = emalloc(data##_len * 2 + 1); \
		data##_len = mysql_real_escape_string(connection, data##_esc, APM_RD_SMART_STRVAL(data), data##_len); \
	} \
}

/* Insert a request in the backend */
static void apm_driver_mysql_insert_request(TSRMLS_D)
{
	APM_DEBUG("[MySQL driver] ======apm_driver_mysql_insert_request \n");
	char *application_esc = NULL, *script_esc = NULL, *uri_esc = NULL, *host_esc = NULL, *cookies_esc = NULL, *post_vars_esc = NULL, *referer_esc = NULL, *method_esc = NULL, *status_esc = NULL, *sql = NULL;
	unsigned int application_len = 0, script_len = 0, uri_len = 0, host_len = 0, ip_int = 0, cookies_len = 0, post_vars_len = 0, referer_len = 0, method_len = 0, status_len = 0;
	struct in_addr ip_addr;
	MYSQL *connection;

	extract_data();

	APM_DEBUG("[MySQL driver] Begin insert request\n");
	if (APM_G(mysql_is_request_created)) {
		APM_DEBUG("[MySQL driver] SKIPPED, request already created.\n");
		return;
	}

	MYSQL_INSTANCE_INIT

	if (APM_G(application_id)) {
		application_len = strlen(APM_G(application_id));
		application_esc = emalloc(application_len * 2 + 1);
		application_len = mysql_real_escape_string(connection, application_esc, APM_G(application_id), application_len);
	}
	
	APM_MYSQL_ESCAPE_STR(script);
	APM_MYSQL_ESCAPE_STR(uri);
	APM_MYSQL_ESCAPE_STR(host);
	APM_MYSQL_ESCAPE_STR(referer);
	APM_MYSQL_ESCAPE_STR(method);
	APM_MYSQL_ESCAPE_STR(status);
	APM_MYSQL_ESCAPE_SMART_STR(cookies);
	APM_MYSQL_ESCAPE_SMART_STR(post_vars);

	if (APM_RD(ip_found) && (inet_pton(AF_INET, APM_RD_STRVAL(ip), &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}

	sql = emalloc(166 + application_len + script_len + uri_len + host_len + cookies_len + post_vars_len + referer_len + method_len);
	sprintf(
		sql,
		"INSERT INTO request (application, script, uri, host, ip, cookies, post_vars, referer, method, status) VALUES ('%s', '%s', '%s', '%s', %u, '%s', '%s', '%s', '%s', '%s')",
		application_esc ? application_esc : "",
		APM_RD(script_found) ? script_esc : "",
		APM_RD(uri_found) ? uri_esc : "",
		APM_RD(host_found) ? host_esc : "",
		ip_int, APM_RD(cookies_found) ? cookies_esc : "",
		APM_RD(post_vars_found) ? post_vars_esc : "",
		APM_RD(referer_found) ? referer_esc : "",
		APM_RD(method_found) ? method_esc : "",
		APM_RD(status_found) ? status_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_G(mysql_event_db)));

	mysql_query(connection, "SET @request_id = LAST_INSERT_ID()");

	efree(sql);
	if (application_esc)
		efree(application_esc);
	if (script_esc)
		efree(script_esc);
	if (uri_esc)
		efree(uri_esc);
	if (host_esc)
		efree(host_esc);
	if (cookies_esc)
		efree(cookies_esc);
	if (post_vars_esc)
		efree(post_vars_esc);
	if (referer_esc)
		efree(referer_esc);
	if (method_esc)
		efree(method_esc);
	if (status_esc)
		efree(status_esc);

	APM_G(mysql_is_request_created) = 1;
	APM_DEBUG("[MySQL driver] End insert request\n");
}

/* Insert an event in the backend */
void apm_driver_mysql_process_event(PROCESS_EVENT_ARGS)
{
	APM_DEBUG("[MySQL driver] ======apm_driver_mysql_process_event \n");
	char *filename_esc = NULL, *msg_esc = NULL, *trace_esc = NULL, *sql = NULL;
	int filename_len = 0, msg_len = 0, trace_len = 0;
	MYSQL *connection;

	apm_driver_mysql_insert_request(TSRMLS_C);

	MYSQL_INSTANCE_INIT

	if (error_filename) {
		filename_len = strlen(error_filename);
		filename_esc = emalloc(filename_len * 2 + 1);
		filename_len = mysql_real_escape_string(connection, filename_esc, error_filename, filename_len);
	}

	if (msg) {
		msg_len = strlen(msg);
		msg_esc = emalloc(msg_len * 2 + 1);
		msg_len = mysql_real_escape_string(connection, msg_esc, msg, msg_len);
	}

	if (trace) {
		trace_len = strlen(trace);
		trace_esc = emalloc(trace_len * 2 + 1);
		trace_len = mysql_real_escape_string(connection, trace_esc, trace, trace_len);
	}

	sql = emalloc(135 + filename_len + msg_len + trace_len);
	sprintf(
		sql,
		"INSERT INTO event (request_id, type, file, line, message, backtrace) VALUES (@request_id, %d, '%s', %u, '%s', '%s')",
		type, error_filename ? filename_esc : "", error_lineno, msg ? msg_esc : "", trace ? trace_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_G(mysql_event_db)));

	efree(sql);
	efree(filename_esc);
	efree(msg_esc);
	efree(trace_esc);
}

int apm_driver_mysql_minit(int module_number TSRMLS_DC)
{
	return SUCCESS;
}

int apm_driver_mysql_rinit(TSRMLS_D)
{
	APM_G(mysql_is_request_created) = 0;
	return SUCCESS;
}

int apm_driver_mysql_mshutdown(SHUTDOWN_FUNC_ARGS)
{
	if (APM_G(mysql_event_db) != NULL) {
		mysql_destroy(TSRMLS_C);
	}

	return SUCCESS;
}

int apm_driver_mysql_rshutdown(TSRMLS_D)
{
	return SUCCESS;
}

void apm_driver_mysql_process_stats(TSRMLS_D)
{
	APM_DEBUG("[MySQL driver] ======apm_driver_mysql_process_stats \n");	
	char *sql = NULL;
	MYSQL *connection;

	apm_driver_mysql_insert_request(TSRMLS_C);

	MYSQL_INSTANCE_INIT

	sql = emalloc(170);
	sprintf(
		sql,
		"INSERT INTO stats (request_id, duration, user_cpu, sys_cpu, mem_peak_usage) VALUES (@request_id, %f, %f, %f, %ld)",
		USEC_TO_SEC(APM_G(duration)),
		USEC_TO_SEC(APM_G(user_cpu)),
		USEC_TO_SEC(APM_G(sys_cpu)),
		APM_G(mem_peak_usage)
	);

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_G(mysql_event_db)));

	efree(sql);
}
