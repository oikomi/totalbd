/*
 * redis.go
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

var redisServiceFuncs []string

func init() {
	redisServiceFuncs = []string{"Redis->connect("}
}


type RedisDiscovery struct {
	RedisInstanceList []string
}

func NewRedisDiscovery() *RedisDiscovery {
	return &RedisDiscovery{
		RedisInstanceList: make([]string, 0),
	}
}

func (m *RedisDiscovery) serviceParse(s string) string {
	//fmt.Println(s)
	// if strings.Contains(s, "mysql_connect(") {
	for _, v := range redisServiceFuncs {
		argsList := strings.Split(strings.TrimRight(strings.TrimLeft(s, v), ")"), ",")
		glog.Info(argsList[0] + argsList[1])

		return argsList[0] + ":" + argsList[1]
	}

	return ""
}

func (rd *RedisDiscovery) Parse(js *simplejson.Json) (*RedisDiscovery, error) {
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
		for _, vv := range redisServiceFuncs {
			if strings.Contains(v, vv) && !strings.Contains(v, "wt") {
				//glog.Info(v)
				redisHost := rd.serviceParse(v)
				if redisHost != "" {
					//mysqlHostSplit := strings.Split(mysqlHost, ":")
					rd.RedisInstanceList = append(rd.RedisInstanceList, redisHost)
				}
			}
		}
	}

	return rd, nil
}
