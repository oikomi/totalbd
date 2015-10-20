/*
 * curl.go
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

var externalServiceFuncs []string

const CURL_OPT string = "CURLOPT_URL"

func init() {
	externalServiceFuncs = []string{"curl_setopt("}
}

type ExternalDiscovery struct {
    ExternalInstanceList    []string
}

func NewExternalDiscovery() *ExternalDiscovery {
    return &ExternalDiscovery {
        ExternalInstanceList : make([]string, 0),
    }
}

func (ed *ExternalDiscovery) serviceParse(s string)  string {
    for _, v := range externalServiceFuncs {
    	argsList := strings.Split(strings.TrimRight(strings.TrimLeft(s, v), ")"), ",")
    	glog.Info(argsList[0] + argsList[1])

    	return argsList[0] + ":" + argsList[1]
	}

	return ""
}

func (ed *ExternalDiscovery) Parse(js *simplejson.Json) (*ExternalDiscovery, error) {
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
        for _, vv := range externalServiceFuncs {
            if strings.Contains(v, vv) && strings.Contains(v, CURL_OPT) && !strings.Contains(v, "wt"){
                //glog.Info(v)
                externalHost := ed.serviceParse(v)
                if externalHost != "" {
    				ed.ExternalInstanceList = append(ed.ExternalInstanceList, externalHost)
                }
            }
        }
    }

    return ed, nil
}
