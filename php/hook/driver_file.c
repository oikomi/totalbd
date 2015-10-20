/*
 * driver_file.c
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#include "php_apm.h"
#include "php_ini.h"

ZEND_EXTERN_MODULE_GLOBALS(apm);

APM_DRIVER_CREATE(file)



void apm_driver_file_process_event(PROCESS_EVENT_ARGS)
{

}

int apm_driver_file_minit(int module_number TSRMLS_DC)
{
	return SUCCESS;
}

int apm_driver_file_rinit(TSRMLS_D)
{

	return SUCCESS;
}

int apm_driver_file_mshutdown(SHUTDOWN_FUNC_ARGS)
{

	return SUCCESS;
}

int apm_driver_file_rshutdown(TSRMLS_D)
{
	return SUCCESS;
}

void apm_driver_file_process_stats(TSRMLS_D)
{

}