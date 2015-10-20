/*
 * stat_data.go
 *
 *  Created on: 20/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package protocol

import (
	"github.com/bitly/go-simplejson"
	"../module"
	"../glog"
)

type Stat struct {
	Time              int64
	Script            string
	Host              string
	Client            string
	Method            string
	Status            string
	Duration          float64
	UserCpuUsage      int64
	SysCpuUsage       int64
	MemPeakUsage      int64

	WebTrace          string
}

func NewStat() *Stat {
	return &Stat {

	}
}

func BuildStat(js *simplejson.Json) (*Stat, error) {
	var webTrace string

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

	tmpMethod, err := js.Get("method").String()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpHost, err := js.Get("host").String()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpClient, err := js.Get("ip").String()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}
	tmpStatus, err := js.Get("status").String()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpDuration, err := js.Get("duration").Float64()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpUserCpuUsage, err := js.Get("user_cpu").Int64()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}
	tmpSysCpuUsage, err := js.Get("sys_cpu").Int64()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}
	tmpMemPeakUsage, err := js.Get("mem_peak_usage").Int64()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	tmpWebTrace := js.Get("web_trace")
	if tmpWebTrace != nil {
		webTrace,err = tmpWebTrace.Get("web_trace_detail").String()
		if err != nil {
			glog.Error(err.Error())
			return nil, err
		}
	}

	s := &Stat {
		Time            : tmpTime,      
		Script          : tmpScript,  
		Host            : tmpHost,
		Client          : tmpClient,  
		Method          : tmpMethod,         
		Status          : tmpStatus,       
		Duration        : tmpDuration / (1000.0 * 1000.0),  
		UserCpuUsage    : tmpUserCpuUsage,   
		SysCpuUsage     : tmpSysCpuUsage,    
		MemPeakUsage    : tmpMemPeakUsage,
		WebTrace        : string(webTrace),	
	}

	return s, nil
}

type StatSummary struct {
	InstanceId          string
	StartReportTime     int64
	AverageRespTime     float64
	TotalReqCount       int64

	CpuUsePercentage    float64
	TotalMemUsage       int64

	Top5Slow            []*Stat
	MysqlData           *module.MysqlData
	MemcacheData        *module.MemcacheData
	MongodbData         *module.MongodbData
	RedisData           *module.RedisData
	ExternalServiceData *module.ExternalServiceData
}

func NewStatsSummary() *StatSummary {
	return &StatSummary {
		Top5Slow  : make([]*Stat, 0),
		MysqlData : module.NewMysqlData(),
		MemcacheData : module.NewMemcacheData(),
		MongodbData  : module.NewMongodbData(),
		ExternalServiceData : module.NewExternalServiceData(),
	}
}