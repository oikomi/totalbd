/*
 * external.go
 *
 *  Created on: 01/09/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

import (
	//"fmt"
	"strconv"
	"strings"

	"../glog"

	"github.com/bitly/go-simplejson"
)

var	gExternalServiceReportData     *ExternalServiceData
var	gExternalServiceTotalResTime   float64

var externalServiceMonitorFuncs []string

func init() {
	externalServiceMonitorFuncs = []string{"curl_exec(", "file_get_contents("}
	gExternalServiceReportData = NewExternalServiceData()
}

func ExternalServiceInitData() {
	gExternalServiceTotalResTime = 0
	gExternalServiceReportData = NewExternalServiceData()
}

type ExternalServiceMonitor struct {
	funcDuration     []map[string]float64
}

func NewExternalServiceMonitor() *ExternalServiceMonitor {
	return &ExternalServiceMonitor {
		funcDuration : make([]map[string]float64, 0),
	}
}

func (c *ExternalServiceMonitor) Parse(js *simplejson.Json) (*ExternalServiceData, error) {
	var err error
	var webTrace string
	tmpWebTrace := js.Get("web_trace")
	if tmpWebTrace != nil {
		webTrace, err = tmpWebTrace.Get("web_trace_detail").String()
		if err != nil {
			glog.Error(err.Error())
			return nil, err
		}
	}

	tmpTime, err := js.Get("ts").Int64()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpScript, err := js.Get("uri").String()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}


	rawDataList := strings.Split(webTrace, "+")

	for _, v := range rawDataList {
		
		for _, vv := range externalServiceMonitorFuncs {
			if strings.Contains(v, vv) && strings.Contains(v, "wt") {
				fd := make(map[string]float64)

				indexWt := strings.Index(v, "wt")
				indexTotal := strings.Index(v, "total")
				
				tmpTotal, err := strconv.ParseFloat(strings.TrimSpace(strings.Split(v[indexTotal:], ":")[1]), 64)
				if err != nil {
					glog.Error(err.Error())
					return nil, err
				}

				fd[v[:indexWt]] = tmpTotal

				gExternalServiceTotalResTime += tmpTotal

				gExternalServiceReportData.TotalReqCount ++ 

				gExternalServiceReportData.AverageRespTime = gExternalServiceTotalResTime / (float64)(gExternalServiceReportData.TotalReqCount)

				externalServiceSlowData := NewExternalServiceSlowData()
				externalServiceSlowData.OpDuration = fd
				externalServiceSlowData.Time = tmpTime
				externalServiceSlowData.Script = tmpScript

				if len(gExternalServiceReportData.Top5Slow) < 5 {
					gExternalServiceReportData.Top5Slow = append(gExternalServiceReportData.Top5Slow, externalServiceSlowData)
				} else {
					for i, val := range gExternalServiceReportData.Top5Slow {
						for _, mapVal := range val.OpDuration {
							if mapVal < tmpTotal {
								gExternalServiceReportData.Top5Slow[i] = externalServiceSlowData
								break
							}					
						}

					}
				}

			}
		}
	}

	//fmt.Println(gExternalServiceReportData)

	return gExternalServiceReportData, nil
}