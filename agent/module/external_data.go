/*
 * external_data.go
 *
 *  Created on: 01/09/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

type ExternalServiceData struct {
	TotalReqCount       int64
	AverageRespTime     float64
	Top5Slow            []*ExternalServiceSlowData
}

func NewExternalServiceData() *ExternalServiceData {
	return &ExternalServiceData {
		Top5Slow  : make([]*ExternalServiceSlowData, 0),
	}
}

type ExternalServiceSlowData struct {
	Time                int64
	Script              string
	OpDuration          map[string]float64
}

func NewExternalServiceSlowData() *ExternalServiceSlowData {
	return &ExternalServiceSlowData {
		OpDuration : make(map[string]float64),
	}
}