/*
 * mysql.go
 *
 *  Created on: 21/09/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package service_discovery

import (
	//"fmt"
	//"strconv"
	"strings"

	"../glog"

	"github.com/bitly/go-simplejson"
)

var mysqlServiceFuncs []string

func init() {
	mysqlServiceFuncs = []string{"mysql_pconnect(", "mysql_connect("}
}

// type MysqlInstance struct {
//     Host       string
//     Port       string
// }

type MysqlDiscovery struct {
	MysqlInstanceList []string
}

func NewMysqlDiscovery() *MysqlDiscovery {
	return &MysqlDiscovery{
		MysqlInstanceList: make([]string, 0),
	}
}

func (m *MysqlDiscovery) serviceParse(s string) string {
	//fmt.Println(s)
	// if strings.Contains(s, "mysql_connect(") {
	for _, v := range mysqlServiceFuncs {
		argsList := strings.Split(strings.TrimLeft(s, v), ",")
		glog.Info(argsList[0])

		return argsList[0]
	}

	return ""
}

func (md *MysqlDiscovery) Parse(js *simplejson.Json) (*MysqlDiscovery, error) {
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

	rawDataList := strings.Split(webTrace, "+")

	for _, v := range rawDataList {
		//glog.Info(v)
		for _, vv := range mysqlServiceFuncs {
			if strings.Contains(v, vv) && strings.Contains(v, "wt") {
				//glog.Info(v)
				mysqlHost := md.serviceParse(v)
				if mysqlHost != "" {
					//mysqlHostSplit := strings.Split(mysqlHost, ":")
					md.MysqlInstanceList = append(md.MysqlInstanceList, mysqlHost)
				}
			}
		}
	}

	return md, nil
}
