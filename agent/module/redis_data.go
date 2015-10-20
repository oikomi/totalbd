/*
 * redis_data.go
 *
 *  Created on: 31/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package module

type RedisData struct {
	TotalReqCount       int64
	AverageRespTime     float64
	Top5Slow            []*RedisSlowData
}

func NewRedisData() *RedisData {
	return &RedisData {
		Top5Slow  : make([]*RedisSlowData, 0),
	}
}

type RedisSlowData struct {
	Time                int64
	Script              string
	OpDuration          map[string]float64
}

func NewRedisSlowData() *RedisSlowData {
	return &RedisSlowData {
		OpDuration : make(map[string]float64),
	}
}
