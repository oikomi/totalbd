// /*
//  * data.go
//  *
//  *  Created on: 13/08/2015
//  *      Author: miaohong(miaohong01@baidu.com)
//  */

package parser

// import (
//  	"sort"
//  	"time"
//  	"strings"
//  	"strconv"
//  	"../glog"
// )

// type StatsSummary struct {
// 	StartReportTime     int64
// 	AverageRespTime     float64
// 	TotalReqCount       int

// 	CpuUsePercentage    float64
// 	TotalMemUsage       int

// 	Top5Slow            []*Stats
// }

// func NewStatsSummary() *StatsSummary {
// 	return &StatsSummary {
// 		Top5Slow : make([]*Stats, 0),
// 	}
// }


// func StatsParse(path string) (*StatsSummary, error) {
// 	s := NewStats()
// 	statsSummary, err := s.parse(path)
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}

// 	return statsSummary, nil
// }


// type Stats struct {
// 	Time              string
// 	Script            string
// 	Host              string
// 	Client            string
// 	Method            string
// 	Status            string
// 	Duration          string
// 	UserCpuUsage      string
// 	SysCpuUsage       string
// 	MemPeakUsage      string
// }

// func NewStats() *Stats {
// 	return &Stats {

// 	}
// }

// func (s *Stats) buildStats(data []string) *Stats {
// 	if len(data) == 10 {
// 		return &Stats {
// 			Time          : strings.TrimSpace(data[0]),
// 			Script        : strings.TrimSpace(data[1]),
// 			Host          : strings.TrimSpace(data[2]),
// 			Client        : strings.TrimSpace(data[3]),
// 			Method        : strings.TrimSpace(data[4]),
// 			Status        : strings.TrimSpace(data[5]),
// 			Duration      : strings.TrimSpace(data[6]),
// 			UserCpuUsage  : strings.TrimSpace(data[7]),
// 			SysCpuUsage   : strings.TrimSpace(data[8]),
// 			MemPeakUsage  : strings.TrimSpace(data[9]),
// 		}
// 	}

// 	return nil
// }


// func (s *Stats) parse(path string) (*StatsSummary, error) {
// 	var err error
// 	var statsSummary *StatsSummary
// 	var averageRespTime float64

// 	var totalCpuUsage  float64
// 	var totalMemPeakUsage  int

// 	statsList := make([]*Stats, 0)

// 	startReportTime := time.Now().Unix()

// 	var totalRespTime float64
// 	df := NewDataFile(path)

// 	err = df.GetAllContent()
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}

// 	err = df.rmFile()
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}


// 	dataList := strings.Split(string(df.content), "\n")

// 	totalReqCount := len(dataList)

// 	for _, data := range dataList {
// 		lineList := strings.Split(string(data), ",")
// 		stats := s.buildStats(lineList)
// 		if stats != nil {
// 			statsList = append(statsList, stats)
// 		}
		
// 		if len(lineList) == 10 {
// 			tmpRespTime , err := strconv.ParseFloat(strings.TrimSpace(lineList[6]), 64)
// 			if err != nil {
// 				glog.Error(err.Error())
// 				return nil, err
// 			}
// 			totalRespTime += tmpRespTime
			
// 			tmpUserCpuUsage , err := strconv.ParseFloat(strings.TrimSpace(lineList[7]), 32)
// 			if err != nil {
// 				glog.Error(err.Error())
// 				return nil, err
// 			}

// 			tmpSysCpuUsage , err := strconv.ParseFloat(strings.TrimSpace(lineList[8]), 32)
// 			if err != nil {
// 				glog.Error(err.Error())
// 				return nil, err
// 			}

// 			totalCpuUsage += tmpUserCpuUsage + tmpSysCpuUsage

			
// 			tmpMemPeakUsage, err := strconv.Atoi(strings.TrimSpace(lineList[9]))
// 			if err != nil{
// 				glog.Error(err.Error())
// 				return nil, err
// 			}

// 			totalMemPeakUsage += tmpMemPeakUsage
// 		}
// 	}

// 	sort.Sort(StatsWrapper{statsList, func (p, q *Stats) bool {
//         tmp1 , err := strconv.ParseFloat(q.Duration, 64) 
// 		if err != nil {
// 			glog.Error(err.Error())
			
// 		}
//         tmp2 , err := strconv.ParseFloat(p.Duration, 64)
// 		if err != nil {
// 			glog.Error(err.Error())
// 		}

//         return tmp1 < tmp2
//     }})

// 	averageRespTime = (totalRespTime / (float64)(totalReqCount))

// 	if len(statsList) <= 5 {
// 		statsSummary = NewStatsSummary()
// 		statsSummary.StartReportTime = startReportTime
// 		statsSummary.AverageRespTime = averageRespTime
// 		statsSummary.TotalReqCount = totalReqCount

// 		statsSummary.CpuUsePercentage = totalCpuUsage / 10.0
// 		statsSummary.TotalMemUsage = totalMemPeakUsage

// 		statsSummary.Top5Slow = statsList
// 	} else {
// 		statsSummary = NewStatsSummary()
// 		statsSummary.StartReportTime = startReportTime
// 		statsSummary.AverageRespTime = averageRespTime
// 		statsSummary.TotalReqCount = totalReqCount
// 		statsSummary.CpuUsePercentage = totalCpuUsage / 10.0
// 		statsSummary.TotalMemUsage = totalMemPeakUsage

// 		for i:=0; i<5; i++ {
// 			statsSummary.Top5Slow = append(statsSummary.Top5Slow, statsList[i])
// 		}	
// 	}

// 	return statsSummary, err
// }

// func EventsParse(path string) (*EventsSummary, error) {
// 	e := NewEvents()
// 	eventsSummary, err := e.parse(path)
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}

// 	return eventsSummary, nil
// }


// type EventsSummary struct {
// 	StartReportTime     int64
// 	EventsList          []*Events
// }


// func NewEventsSummary() *EventsSummary {
// 	return &EventsSummary {
// 		EventsList : make([]*Events, 0),
// 	}
// }

// type Events struct {
// 	Time     string
// 	Script   string
// 	Msg      string
// 	Trace    string
// }

// func NewEvents() *Events {
// 	return &Events {

// 	}
// }

// func (e *Events) buildEvents(data []string) *Events {
// 	if len(data) == 4 {
// 		return &Events {
// 			Time   : strings.TrimSpace(data[0]),
// 			Script : strings.TrimSpace(data[1]),
// 			Msg    : strings.TrimSpace(data[2]),
// 			Trace  : strings.TrimSpace(data[3]),
// 		}
// 	}

// 	return nil
// }


// func (e *Events) parse(path string) (*EventsSummary, error){
// 	var eventsSummary *EventsSummary

// 	startReportTime := time.Now().Unix()

// 	eventsList := make([]*Events, 0)
// 	df := NewDataFile(path)
// 	err := df.GetAllContent()
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}
// 	err = df.rmFile()
// 	if err != nil {
// 		glog.Error(err.Error())
// 		return nil, err
// 	}

// 	dataList := strings.Split(string(df.content), "\n")
// 	for _, data := range dataList {
// 		lineList := strings.Split(string(data), "+")
// 		events := e.buildEvents(lineList)
// 		if events != nil {
// 			eventsList = append(eventsList, events)
// 		}

// 	}

// 	eventsSummary = NewEventsSummary()

// 	eventsSummary.StartReportTime = startReportTime
// 	eventsSummary.EventsList = eventsList

// 	return eventsSummary ,nil
// }