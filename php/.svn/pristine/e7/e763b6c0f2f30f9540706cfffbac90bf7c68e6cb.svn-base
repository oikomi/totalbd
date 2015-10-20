/*
 * backtrace.h
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#ifndef BACKTRACE_H
#define BACKTRACE_H

#if PHP_VERSION_ID >= 70000
# include "zend_smart_str.h"
#else
# include "ext/standard/php_smart_str.h"
#endif


void append_backtrace(smart_str *trace_str TSRMLS_DC);

#endif

