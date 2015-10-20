/*
 * mongodb.go
 *
 *  Created on: 22/09/2015
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

var mongodbServiceFuncs []string

func init() {
	mongodbServiceFuncs = []string{"MongoClient->__construct("}
}


type MongodbDiscovery struct {
    MongodbInstanceList    []string
}


func NewMongodbDiscovery() *MongodbDiscovery {
    return &MongodbDiscovery {
        MongodbInstanceList : make([]string, 0),
    }
}

func (md *MongodbDiscovery) serviceParse(s string)  string {
	//fmt.Println(s)
	// if strings.Contains(s, "mysql_connect(") {
    for _, v := range mongodbServiceFuncs {
    	argsList := strings.Split(strings.TrimRight(strings.TrimLeft(s, v), ")"), ",")
    	//glog.Info(argsList[0] + argsList[1])

    	return argsList[0]
	}

	return ""
}

func (md *MongodbDiscovery) Parse(js *simplejson.Json) (*MongodbDiscovery, error) {
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
        for _, vv := range mongodbServiceFuncs {
            if strings.Contains(v, vv) && !strings.Contains(v, "wt") {
                //glog.Info(v)
                mongodbHost := md.serviceParse(v)
                if mongodbHost != "" {
    				md.MongodbInstanceList = append(md.MongodbInstanceList, mongodbHost)
                }
            }
        }
    }

    return md, nil
}
