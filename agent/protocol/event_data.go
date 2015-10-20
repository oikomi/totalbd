
/*
 * event_data.go
 *
 *  Created on: 20/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */


package protocol

import (
 	//"fmt"
	"github.com/bitly/go-simplejson"

	"../glog"
)


type EventSummary struct {
	StartReportTime     int64
	EventList          []*Event
}


func NewEventSummary() *EventSummary {
	return &EventSummary {
		EventList : make([]*Event, 0),
	}
}

type Event struct {
	Time     int64
	Script   string
	Msg      string
	Trace    string
}

func NewEvent() *Event {
	return &Event {

	}
}

func BuildEvent(js *simplejson.Json) ([]*Event, error) {
	eventList := make([]*Event, 0)

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

	tmpErrorList := js.Get("errors")
	// if err != nil {
	// 	glog.Error(err.Error())
	// 	return nil, err
	// }
	l, err := tmpErrorList.Array()
	if err != nil {
		glog.Error(err.Error())
		return nil, err
	}

	for i := 0; i < len(l); i++ {
		tmpErrorList.GetIndex(i)
		tmpMsg, err := tmpErrorList.GetIndex(i).Get("message").String()
		if err != nil {
			glog.Error(err.Error())
			return nil, err
		}

		tmpTrace, err := tmpErrorList.GetIndex(i).Get("trace").String()
		if err != nil {
			glog.Error(err.Error())
			return nil, err
		}

		e := &Event {
			Time     : tmpTime,
			Script   : tmpScript,
			Msg      : tmpMsg,
			Trace    : tmpTrace,				
		}

		eventList = append(eventList, e)
	}
	// for _, v := range tmpErrorList {
	// 	fmt.Println(v)
	// 	//fmt.Println(v.(type))

	// 	tmpMsg, err := v.(map[string]interface{}).Get("message").String()
	// 	if err != nil {
	// 		glog.Error(err.Error())
	// 		return nil, err
	// 	}

	// 	tmpTrace, err := v.(map[string]interface{}).Get("trace").String()
	// 	if err != nil {
	// 		glog.Error(err.Error())
	// 		return nil, err
	// 	}

	// 	e := &Event {
	// 		Time     : tmpTime,
	// 		Script   : tmpScript,
	// 		Msg      : tmpMsg,
	// 		Trace    : tmpTrace,				
	// 	}
	// 	fmt.Println("----")
	// 	fmt.Println(e)
	// 	fmt.Println("----")

	// 	eventList = append(eventList, e)



	// 	// switch v.(type) {
	// 	// case *simplejson.Json:
	// 	// 	tmpMsg, err := v.(*simplejson.Json).Get("message").String()
	// 	// 	if err != nil {
	// 	// 		glog.Error(err.Error())
	// 	// 		return nil, err
	// 	// 	}

	// 	// 	tmpTrace, err := v.(*simplejson.Json).Get("trace").String()
	// 	// 	if err != nil {
	// 	// 		glog.Error(err.Error())
	// 	// 		return nil, err
	// 	// 	}

	// 	// 	e := &Event {
	// 	// 		Time     : tmpTime,
	// 	// 		Script   : tmpScript,
	// 	// 		Msg      : tmpMsg,
	// 	// 		Trace    : tmpTrace,				
	// 	// 	}
	// 	// 	fmt.Println("----")
	// 	// 	fmt.Println(e)
	// 	// 	fmt.Println("----")

	// 	// 	eventList = append(eventList, e)

	// 	// }

	// }

	// tmpMsg, err := js.Get("errors").Get("message").String()
	// if err != nil {
	// 	glog.Error(err.Error())
	// 	return nil, err
	// }

	// tmpTrace, err := js.Get("errors").Get("trace").String()
	// if err != nil {
	// 	glog.Error(err.Error())
	// 	return nil, err
	// }

	return eventList, err
}