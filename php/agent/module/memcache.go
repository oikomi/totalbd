/*
 * memcache.go
 *
 *  Created on: 31/08/2015
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

var	gMemcacheReportData     *MemcacheData
var	gMemcacheTotalResTime   float64

var memcacheMonitorFuncs []string

func init() {
	memcacheMonitorFuncs = []string{"Memcache->set(", "Memcache->get("}
	gMemcacheReportData = NewMemcacheData()
}

func MemcacheInitData() {
	gMemcacheTotalResTime = 0
	gMemcacheReportData = NewMemcacheData()
}

type MemcacheMonitor struct {
	funcDuration     []map[string]float64
}

func NewMemcacheMonitor() *MemcacheMonitor {
	return &MemcacheMonitor {
		funcDuration : make([]map[string]float64, 0),
	}
}

func (m *MemcacheMonitor) Parse(js *simplejson.Json) (*MemcacheData, error) {
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
		
		for _, vv := range memcacheMonitorFuncs {
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

				gMemcacheTotalResTime += tmpTotal

				gMemcacheReportData.TotalReqCount ++ 

				gMemcacheReportData.AverageRespTime = gMemcacheTotalResTime / (float64)(gMemcacheReportData.TotalReqCount)

				memcacheSlowData := NewMemcacheSlowData()
				memcacheSlowData.OpDuration = fd
				memcacheSlowData.Time = tmpTime
				memcacheSlowData.Script = tmpScript

				if len(gMemcacheReportData.Top5Slow) < 5 {
					gMemcacheReportData.Top5Slow = append(gMemcacheReportData.Top5Slow, memcacheSlowData)
				} else {
					for i, val := range gMemcacheReportData.Top5Slow {
						for _, mapVal := range val.OpDuration {
							if mapVal < tmpTotal {
								gMemcacheReportData.Top5Slow[i] = memcacheSlowData
								break
							}					
						}

					}
				}

			}
		}
	}

	//fmt.Println(gMemcacheReportData)

	return gMemcacheReportData, nil
}
