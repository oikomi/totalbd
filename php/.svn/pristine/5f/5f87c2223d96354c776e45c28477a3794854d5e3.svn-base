/*
 * mongodb.go
 *
 *  Created on: 06/09/2015
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

var	gMongodbReportData     *MongodbData
var	gMongodbTotalResTime   float64

var mongodbMonitorFuncs []string

func init() {
	mongodbMonitorFuncs = []string{"MongoCollection->insert(", "MongoCollection->find("}
	gMongodbReportData = NewMongodbData()
}

func MongodbInitData() {
	gMongodbTotalResTime = 0
	gMongodbReportData = NewMongodbData()
}

type MongodbMonitor struct {
	funcDuration     []map[string]float64
}

func NewMongodbMonitor() *MongodbMonitor {
	return &MongodbMonitor {
		funcDuration : make([]map[string]float64, 0),
	}
}

func (m *MongodbMonitor) Parse(js *simplejson.Json) (*MongodbData, error) {
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
		
		for _, vv := range mongodbMonitorFuncs {
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

				gMongodbTotalResTime += tmpTotal

				gMongodbReportData.TotalReqCount ++ 

				gMongodbReportData.AverageRespTime = gMongodbTotalResTime / (float64)(gMongodbReportData.TotalReqCount)

				mongodbSlowData := NewMongodbSlowData()
				mongodbSlowData.OpDuration = fd
				mongodbSlowData.Time = tmpTime
				mongodbSlowData.Script = tmpScript

				if len(gMongodbReportData.Top5Slow) < 5 {
					gMongodbReportData.Top5Slow = append(gMongodbReportData.Top5Slow, mongodbSlowData)
				} else {
					for i, val := range gMongodbReportData.Top5Slow {
						for _, mapVal := range val.OpDuration {
							if mapVal < tmpTotal {
								gMongodbReportData.Top5Slow[i] = mongodbSlowData
								break
							}					
						}

					}
				}

			}
		}
	}

	//fmt.Println(gMongodbReportData)

	return gMongodbReportData, nil
}
