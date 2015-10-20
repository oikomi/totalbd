/*
 * memcache_data.go
 *
 *  Created on: 31/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

type MemcacheData struct {
	TotalReqCount       int64
	AverageRespTime     float64
	Top5Slow            []*MemcacheSlowData
}

func NewMemcacheData() *MemcacheData {
	return &MemcacheData {
		Top5Slow  : make([]*MemcacheSlowData, 0),
	}
}

type MemcacheSlowData struct {
	Time                int64
	Script              string
	OpDuration          map[string]float64
}

func NewMemcacheSlowData() *MemcacheSlowData {
	return &MemcacheSlowData {
		OpDuration : make(map[string]float64),
	}
}
