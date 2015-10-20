/*
 * mysql.go
 *
 *  Created on: 31/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

import (
	"fmt"
 	//"time"
	"strconv"
	"strings"

	"../glog"

	"github.com/bitly/go-simplejson"
)

var	gMysqlreportData   *MysqlData
var	gMysqlTotalResTime   float64

var mysqlMonitorFuncs []string
// type mysqlOprationDuration map[string]float64
// var mysqlOprationClassify map[string]mysqlOprationDuration

func init() {
	mysqlMonitorFuncs = []string{"mysql_query("}

	gMysqlreportData = NewMysqlData()
}

type MysqlMonitor struct {
	mysqlData     *MysqlData
}

func NewMysqlMonitor() *MysqlMonitor {
	return &MysqlMonitor {
		mysqlData    : NewMysqlData(),
	}
}

func MysqlInitData() {
	gMysqlTotalResTime = 0
	gMysqlreportData = NewMysqlData()
}

func (m *MysqlMonitor) sqlParse(s string)  string {
	//fmt.Println(s)
	if strings.Contains(s, "mysql_query(") {
		argsList := strings.Split(strings.TrimLeft(s, "mysql_query("), ",")
		fmt.Println(argsList[0])

		return argsList[0]
		// if strings.Contains(strings.ToLower(argsList[0]), "select") {
		// 	//gMysqlreportData.MysqlOprationClassify["SELECT"] = 
			
		// }
	}

	return ""

}

func (m *MysqlMonitor) Parse(js *simplejson.Json) (*MysqlData, error) {
	var err error
	var webTrace string
	tmpWebTrace := js.Get("web_trace")
	if tmpWebTrace != nil {
		webTrace,err = tmpWebTrace.Get("web_trace_detail").String()
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
		for _, vv := range mysqlMonitorFuncs {
			if strings.Contains(v, vv) && strings.Contains(v, "wt") {
				//fmt.Println(v)
				//parse args
				sqlStatement := m.sqlParse(v)

				fd := make(map[string]float64)

				indexWt := strings.Index(v, "wt")
				indexTotal := strings.Index(v, "total")
				
				tmpTotal, err := strconv.ParseFloat(strings.TrimSpace(strings.Split(v[indexTotal:], ":")[1]), 64)
				if err != nil {
					glog.Error(err.Error())
					return nil, err
				}
				if sqlStatement != "" {
					fd[sqlStatement] = tmpTotal
				} else {
					fd[v[:indexWt]] = tmpTotal
				}

				gMysqlTotalResTime += tmpTotal

				gMysqlreportData.TotalReqCount ++ 

				gMysqlreportData.AverageRespTime = gMysqlTotalResTime / (float64)(gMysqlreportData.TotalReqCount)

				mysqlSlowData := NewMysqlSlowData()
				mysqlSlowData.SqlDuration = fd
				mysqlSlowData.Time = tmpTime
				mysqlSlowData.Script = tmpScript


				if len(gMysqlreportData.Top5Slow) < 5 {
					gMysqlreportData.Top5Slow = append(gMysqlreportData.Top5Slow, mysqlSlowData)
				} else {
					for i, val := range gMysqlreportData.Top5Slow {
						for _, mapVal := range val.SqlDuration {
							if mapVal < tmpTotal {
								gMysqlreportData.Top5Slow[i] = mysqlSlowData
								break
							}					
						}

					}
				}

			} 
			if strings.Contains(v, vv) && ! strings.Contains(v, "wt") {
				//parse arguments
				//m.argumentsParse(v)
			}
		}
	}

	//fmt.Println(gMysqlreportData)

	return gMysqlreportData, nil
}
