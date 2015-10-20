/*
 * event_data.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */


package parser

import (
 	"time"
 	"strings"
 	"../glog"
)

func EventsParse(path string) (*EventsSummary, error) {
	e := NewEvents()
	eventsSummary, err := e.parse(path)
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	return eventsSummary, nil
}


type EventsSummary struct {
	StartReportTime     int64
	EventsList          []*Events
}


func NewEventsSummary() *EventsSummary {
	return &EventsSummary {
		EventsList : make([]*Events, 0),
	}
}

type Events struct {
	Time     string
	Script   string
	Msg      string
	Trace    string
}

func NewEvents() *Events {
	return &Events {

	}
}

func (e *Events) buildEvents(data []string) *Events {
	if len(data) == 4 {
		return &Events {
			Time   : strings.TrimSpace(data[0]),
			Script : strings.TrimSpace(data[1]),
			Msg    : strings.TrimSpace(data[2]),
			Trace  : strings.TrimSpace(data[3]),
		}
	}

	return nil
}


func (e *Events) parse(path string) (*EventsSummary, error) {
	var eventsSummary *EventsSummary

	startReportTime := time.Now().Unix()

	eventsList := make([]*Events, 0)
	df := NewDataFile(path)
	err := df.GetAllContent()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}
	err = df.rmFile()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	dataList := strings.Split(string(df.content), "\n")
	for _, data := range dataList {
		lineList := strings.Split(string(data), "+")
		events := e.buildEvents(lineList)
		if events != nil {
			eventsList = append(eventsList, events)
		}

	}

	eventsSummary = NewEventsSummary()

	eventsSummary.StartReportTime = startReportTime
	eventsSummary.EventsList = eventsList

	return eventsSummary ,nil
}