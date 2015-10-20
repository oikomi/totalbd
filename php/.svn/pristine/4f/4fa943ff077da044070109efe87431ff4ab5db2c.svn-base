/*
 * agent.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package main

import (
 	"fmt"
 	"flag"
 	"time"
 	"strings"
 	"encoding/json"
	"./glog"
	"./parser"
	"./proxy"
	"./protocol"
)

/*
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
const char* build_time(void) {
	static const char* psz_build_time = "["__DATE__ " " __TIME__ "]";
	return psz_build_time;
}
*/
import "C"

var (
	buildTime = C.GoString(C.build_time())
)

func BuildTime() string {
	return buildTime
}

const VERSION string = "1.0.0"

func version() {
	fmt.Printf("php agent version %s \n", VERSION)
}

func init() {
	flag.Set("alsologtostderr", "false")
	flag.Set("log_dir", "/tmp/agent_log")
}

var InputConfFile = flag.String("conf_file", "agent.json", "input conf file name") 
//var logFile = flag.String("log_dir", "/tmp/agent.log", "log file name") 

func doParseEventsData(cfg *AgentConfig) {
	events, err := parser.EventsParse(cfg.EventsDataPath)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	b, err := json.Marshal(events)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	r := proxy.NewReqHttp(cfg.ReportServer + cfg.EventsDataService, "POST", 5)
	r.SetHeader("Content-Type", "application/json")
	r.SetHeader("X-App-License-Key", "e3550f68961d4bb3b14b777f347e7c15")
	//glog.Info(string(b))
	
	err = r.DoPostData(b)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	return
}

func doParseStatsData(cfg *AgentConfig) {
	stats, err := parser.StatsParse(cfg.StatsDataPath)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	b, err := json.Marshal(stats)
	if err != nil {
		glog.Error(err.Error())
		return
	}
	glog.Info(string(b))

	r := proxy.NewReqHttp(cfg.ReportServer + cfg.StatsDataService, "POST", 5)
	r.SetHeader("Content-Type", "application/json")
	r.SetHeader("X-App-License-Key", "e3550f68961d4bb3b14b777f347e7c15")

	err = r.DoPostData(b)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	return
}

func doFileProc(cfg *AgentConfig) {
	timer := time.NewTicker(cfg.ParseDataInterval * time.Second)
	ttl := time.After(cfg.ParseDataExpire * time.Second)
	for {
		select {
		case <-timer.C:	
			go doParseStatsData(cfg)
			go doParseEventsData(cfg)
		case <-ttl:
			break
		}
	}
}

func parseProtocol(cfg *AgentConfig) {
	l := strings.Split(cfg.Protocol, "|")

	switch l[0] {
	case "unix":
		protocol.DoUnixProc(l[1])
	case "file":
		doFileProc(cfg)

	}
}

func main() {
	version()
	fmt.Printf("built on %s\n", BuildTime())
	flag.Parse()
	cfg := NewAgentConfig(*InputConfFile)
	err := cfg.LoadConfig()
	if err != nil {
		glog.Error(err.Error())
		return
	}

	parseProtocol(cfg)

	glog.Flush()
}