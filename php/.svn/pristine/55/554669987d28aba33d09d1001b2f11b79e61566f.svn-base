/*
 * service_discovery.go
 *
 *  Created on: 21/09/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package service_discovery

import (
	//"fmt"
	//"strconv"
	//"strings"

	"../glog"

	"github.com/bitly/go-simplejson"
)

var	gServiceDiscoveryData     *ServiceDiscoveryData

func init() {
	gServiceDiscoveryData = NewServiceDiscoveryData()
}

func ServiceDiscoveryInitData() {
	gServiceDiscoveryData = NewServiceDiscoveryData()
}

type ServiceDiscoveryData struct {
    MysqlDiscovery     *MysqlDiscovery
    RedisDiscovery     *RedisDiscovery
    MemcacheDiscovery  *MemcacheDiscovery
    MongodbDiscovery   *MongodbDiscovery
    ExternalDiscovery  *ExternalDiscovery
}

func NewServiceDiscoveryData() *ServiceDiscoveryData {
    return &ServiceDiscoveryData {
        MysqlDiscovery : NewMysqlDiscovery(),
        RedisDiscovery : NewRedisDiscovery(),
        MemcacheDiscovery : NewMemcacheDiscovery(),
        MongodbDiscovery : NewMongodbDiscovery(),

        ExternalDiscovery : NewExternalDiscovery(),
    }
}

func (s *ServiceDiscoveryData) Parse(js *simplejson.Json) (*ServiceDiscoveryData, error) {
    mysqlDiscovery := NewMysqlDiscovery()
    mysqlDiscoveryData, err := mysqlDiscovery.Parse(js)
    if err != nil {
        glog.Error(err.Error())
        return nil, err
    }

    gServiceDiscoveryData.MysqlDiscovery = mysqlDiscoveryData

    redisDiscovery := NewRedisDiscovery()
    redisDiscoveryData, err := redisDiscovery.Parse(js)
    if err != nil {
        glog.Error(err.Error())
        return nil, err
    }

    gServiceDiscoveryData.RedisDiscovery = redisDiscoveryData

    memcacheDiscovery := NewMemcacheDiscovery()
    memcacheDiscoveryData, err := memcacheDiscovery.Parse(js)
    if err != nil {
        glog.Error(err.Error())
        return nil, err
    }

    gServiceDiscoveryData.MemcacheDiscovery = memcacheDiscoveryData

    mongodbDiscovery := NewMongodbDiscovery()
    mongodbDiscoveryData, err := mongodbDiscovery.Parse(js)
    if err != nil {
        glog.Error(err.Error())
        return nil, err
    }

    gServiceDiscoveryData.MongodbDiscovery = mongodbDiscoveryData

    externalDiscovery := NewExternalDiscovery()
    externalDiscoveryData, err := externalDiscovery.Parse(js)
    if err != nil {
        glog.Error(err.Error())
        return nil, err
    }

    gServiceDiscoveryData.ExternalDiscovery = externalDiscoveryData

    return gServiceDiscoveryData, nil
}
