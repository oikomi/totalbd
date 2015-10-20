/*
 * agent_config.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package main

import (
	"os"
	"time"
	"encoding/json"
	"./glog"
)

type AgentConfig struct {
	configFile            string
	Protocol              string
	ReportServer          string
	LogFile               string

	StatsDataPath         string
	StatsDataService      string

	EventsDataPath        string
	EventsDataService     string

	WebTraceDataPath      string
	WebTracesDataService  string

	ParseDataInterval     time.Duration
	ParseDataExpire       time.Duration
}

func NewAgentConfig(configFile string) *AgentConfig {
	return &AgentConfig {
		configFile : configFile,
	}
}

func (self *AgentConfig)LoadConfig() error {
	file, err := os.Open(self.configFile)
	if err != nil {
		glog.Error(err.Error())
		return err
	}
	defer file.Close()

	dec := json.NewDecoder(file)
	err = dec.Decode(&self)
	if err != nil {
		return err
	}
	return nil
}

func (self *AgentConfig)DumpConfig() {
	//fmt.Printf("Mode: %s\nListen: %s\nServer: %s\nLogfile: %s\n", 
	//cfg.Mode, cfg.Listen, cfg.Server, cfg.Logfile)
}
