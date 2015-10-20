/*
 * redis.go
 *
 *  Created on: 08/09/2015
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

var	gRedisReportData     *RedisData
var	gRedisTotalResTime   float64

var redisMonitorFuncs []string

func init() {
	redisMonitorFuncs = []string{"Redis->set(", "Redis->get("}
	gRedisReportData = NewRedisData()
}

func RedisInitData() {
	gRedisTotalResTime = 0
	gRedisReportData = NewRedisData()
}

type RedisMonitor struct {
	funcDuration     []map[string]float64
}

func NewRedisMonitor() *RedisMonitor {
	return &RedisMonitor {
		funcDuration : make([]map[string]float64, 0),
	}
}

func (m *RedisMonitor) Parse(js *simplejson.Json) (*RedisData, error) {
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
		
		for _, vv := range redisMonitorFuncs {
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


				gRedisTotalResTime += tmpTotal

				gRedisReportData.TotalReqCount ++ 

				gRedisReportData.AverageRespTime = gRedisTotalResTime / (float64)(gRedisReportData.TotalReqCount)

				redisSlowData := NewRedisSlowData()
				redisSlowData.OpDuration = fd
				redisSlowData.Time = tmpTime
				redisSlowData.Script = tmpScript

				if len(gRedisReportData.Top5Slow) < 5 {
					gRedisReportData.Top5Slow = append(gRedisReportData.Top5Slow, redisSlowData)
				} else {
					for i, val := range gRedisReportData.Top5Slow {
						for _, mapVal := range val.OpDuration {
							if mapVal < tmpTotal {
								gRedisReportData.Top5Slow[i] = redisSlowData
								break
							}					
						}

					}
				}

			}
		}
	}

	//fmt.Println(gRedisReportData)

	return gRedisReportData, nil
}
