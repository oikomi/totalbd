/*
 * unix.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package protocol

import (
 	"os"
 	"fmt"
 	"net"
 	"time"
 	"sort"
 	//"strconv"
 	//"strings"
 	"encoding/json"
 	"github.com/bitly/go-simplejson"
	"../glog"
	"../proxy"
	"../module"
)

func DoUnixProc(addr string) error {
	up := NewUnixProto(addr)
	err := up.deleteSock()
    if err != nil {
    	glog.Error(err.Error())
		return err
    } 

    up.run()

    return err
}

var	greportStatData   *StatSummary
var	greportEventData  *EventSummary
var gtotalRespTime    int64
var gtotalCpuUsage    int64
//var gtotalMemUsage    int64

func init() {
	greportStatData  = NewStatsSummary()
	greportEventData = NewEventSummary()
}

type UnixProto struct {
	listener         *net.UnixListener
	addr             string

	conn             *net.UnixConn

	// reportStatData   *StatSummary
	// reportEventData  *EventSummary
}

func NewUnixProto(addr string) *UnixProto {
	return &UnixProto {
		addr : addr,
		// reportStatData  : NewStatsSummary(),
		// reportEventData : NewEventSummary(),
	}
}

func initData() {
	gtotalRespTime = 0
	gtotalCpuUsage = 0	
}

func ReportStatData() {
	r := proxy.NewReqHttp("http://sdkbackend-test.jpaas-off00.baidu.com" + "/server/php/performance", "POST", 5)
	r.SetHeader("Content-Type", "application/json")
	r.SetHeader("X-App-License-Key", "e3550f68961d4bb3b14b777f347e7c15")

	tmpHostName, err := os.Hostname()
	if err != nil {
		glog.Error(err.Error())
		return 
	}	

	greportStatData.InstanceId = tmpHostName

	// glog.Info(string(b))
	greportStatData.StartReportTime = time.Now().Unix()
	greportStatData.AverageRespTime = greportStatData.AverageRespTime / (1000.0 * 1000.0 * 1000.0)
	greportStatData.CpuUsePercentage = (float64)(gtotalCpuUsage) / (10.0 * 1000 * 1000)

	b, err := json.Marshal(greportStatData)
	if err != nil {
		glog.Error(err.Error())
		return
	}
	fmt.Println(string(b))
	greportStatData  = NewStatsSummary()
	initData()

	//reset status
	module.MysqlInitData()
	module.MemcacheInitData()
	module.MongodbInitData()
	module.RedisInitData()
	module.ExternalServiceInitData()

	err = r.DoPostData(b)
	if err != nil {
		glog.Error(err.Error())
		return
	}
}

func ReportEventData() {
	r := proxy.NewReqHttp("http://sdkbackend-test.jpaas-off00.baidu.com" + "/server/php/performance", "POST", 5)
	r.SetHeader("Content-Type", "application/json")
	r.SetHeader("X-App-License-Key", "e3550f68961d4bb3b14b777f347e7c15")	

	b, err := json.Marshal(greportEventData)
	if err != nil {
		glog.Error(err.Error())
		return
	}

	//fmt.Println(string(b))

	greportEventData = NewEventSummary()

	err = r.DoPostData(b)
	if err != nil {
		glog.Error(err.Error())
		return
	}	
}

func (u *UnixProto) contentParse(data []byte) error {
	//fmt.Println(string(data))
	js, err := simplejson.NewJson(data)
	if err != nil {
		glog.Error(err.Error())
		return err
	}	

	tmpDuration, err := js.Get("duration").Int64()
	if err != nil {
		glog.Error(err.Error())
		return err
	}

	tmpUserCpu, err := js.Get("user_cpu").Int64()
	if err != nil {
		glog.Error(err.Error())
		return err
	}

	tmpSysCpu, err := js.Get("sys_cpu").Int64()
	if err != nil {
		glog.Error(err.Error())
		return err
	}

	tmpMemPeakUsage, err := js.Get("mem_peak_usage").Int64()
	if err != nil {
		glog.Error(err.Error())
		return err
	}

	gtotalRespTime += tmpDuration
	greportStatData.TotalReqCount ++

	greportStatData.TotalMemUsage += tmpMemPeakUsage

	greportStatData.AverageRespTime += (float64)(gtotalRespTime / greportStatData.TotalReqCount)

	gtotalCpuUsage += tmpUserCpu
	gtotalCpuUsage += tmpSysCpu

	s, err := BuildStat(js)
	if err != nil {
		glog.Error(err.Error())
		return err
	}

	//add for db and external services
	if len(s.WebTrace) != 0 {
		mysqlMonitor := module.NewMysqlMonitor()
		greportStatData.MysqlData, err = mysqlMonitor.Parse(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}

		memcacheMonitor := module.NewMemcacheMonitor()
		greportStatData.MemcacheData, err = memcacheMonitor.Parse(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}

		mongodbMonitor := module.NewMongodbMonitor()
		greportStatData.MongodbData, err = mongodbMonitor.Parse(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}

		redisMonitor := module.NewRedisMonitor()
		greportStatData.RedisData, err = redisMonitor.Parse(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}

		externalServiceMonitor := module.NewExternalServiceMonitor()
		greportStatData.ExternalServiceData, err = externalServiceMonitor.Parse(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}	

	}

	if len(greportStatData.Top5Slow) == 5 {
		sort.Sort(StatsWrapper{greportStatData.Top5Slow, func (p, q *Stat) bool {

	        return p.Duration > q.Duration
	    }})
	}

	if len(greportStatData.Top5Slow) < 5 {
		greportStatData.Top5Slow = append(greportStatData.Top5Slow, s)
	} else {
		for i := 0; i < 5; i++ {
			if greportStatData.Top5Slow[i].Duration < s.Duration {
				greportStatData.Top5Slow[i] = s
				break
			}
		} 
	}

	_, err = js.Get("errors").Array()
	if err == nil {
		el, err := BuildEvent(js)
		if err != nil {
			glog.Error(err.Error())
			return err
		}

		greportEventData.EventList = append(greportEventData.EventList, el...)
	}

	return err
}

func (u *UnixProto) deleteSock() error {
	_, err := os.Stat(u.addr)
	if err != nil && os.IsNotExist(err) {
		
	} else {
		err := os.Remove(u.addr)
	    if err != nil {
	    	glog.Error(err.Error())
			return err
	    } 
	}

	return err
}

func (u *UnixProto) run() {
	var err error
	u.listener, err = net.ListenUnix("unix",  &net.UnixAddr{u.addr, "unix"})
	if err != nil {
	    panic(err)
	}
	defer os.Remove(u.addr)

	timer := time.NewTicker(10 * time.Second)
	ttl := time.After(60 * time.Second)

	go func(){
		for {
			select {
			case <-timer.C:	
				go ReportStatData()
				go ReportEventData()
			case <-ttl:
				break
			}
		}
	}()

	for {
	    u.conn, err = u.listener.AcceptUnix()
	    if err != nil {
	        panic(err)
	    }
	    //var buf [8*1024]byte
	    buf := make([]byte, 64*1024)
	    _, err = u.conn.Read(buf[:])
	    if err != nil {
	        panic(err)
	    }
	    u.contentParse(buf)
	}

	defer u.conn.Close()
}

