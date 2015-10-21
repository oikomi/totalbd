/*
 * driver_mysql.h
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#ifndef DRIVER_MYSQL_H
#define DRIVER_MYSQL_H

#define DB_HOST "localhost"
#define DB_USER "root"

#include "php_apm.h"

#define APM_E_mysql APM_E_ALL

#define MYSQL_INSTANCE_INIT_EX(ret) connection = mysql_get_instance(TSRMLS_C); \
	if (connection == NULL) { \
		return ret; \
	}
#define MYSQL_INSTANCE_INIT MYSQL_INSTANCE_INIT_EX()

apm_driver_entry * apm_driver_mysql_create();

PHP_INI_MH(OnUpdateAPMmysqlErrorReporting);

#endif
