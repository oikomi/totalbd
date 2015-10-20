/*
 * mongodb_data.go
 *
 *  Created on: 06/09/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

type MongodbData struct {
	TotalReqCount       int64
	AverageRespTime     float64
	Top5Slow            []*MongodbSlowData
}

func NewMongodbData() *MongodbData {
	return &MongodbData {
		Top5Slow  : make([]*MongodbSlowData, 0),
	}
}

type MongodbSlowData struct {
	Time                int64
	Script              string
	OpDuration          map[string]float64
}

func NewMongodbSlowData() *MongodbSlowData {
	return &MongodbSlowData {
		OpDuration : make(map[string]float64),
	}
}
