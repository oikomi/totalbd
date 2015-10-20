/*
 * unix_raw_data.go
 *
 *  Created on: 20/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package protocol

type WebError struct {
	Type     string  `json:"type"` 
	Line     string  `json:"line"` 
	File     string  `json:"file"` 
	Message  string  `json:"message"` 
	Trace    string  `json:"trace"` 
}

type WebTrace struct {
	WebTraceDetail   string  `json:"web_trace_detail"`
}

type UnixRawData struct {
	ApplicationId    string  `json:"application_id"` 
	ResponseCode     string  `json:"response_code"` 
	Ts               int64   `json:"ts"` 
	Script           string  `json:"script"` 
	Uri              string  `json:"uri"` 
	Host             string  `json:"host"` 
	Status           string  `json:"status"` 
	Ip               string  `json:"ip"` 
	Method           string  `json:"method"` 
	Duration         float64 `json:"duration"` 
	MemPeakUsage     string  `json:"mem_peak_usage"` 
	UserCpu          int64   `json:"user_cpu"` 
	SysCpu           int64   `json:"sys_cpu"` 

	Errors           []WebError `json:"errors"` 
	WebTrace         WebTrace  `json:"web_trace"` 
}