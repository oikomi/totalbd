/*
 * util.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */


package parser

import (
	"sort"
)

type SortBy func(p, q *Stats) bool

type StatsWrapper struct {
    stats []*Stats
    by func(p, q * Stats) bool
}

func (sw StatsWrapper) Len() int {
    return len(sw.stats)
}
func (sw StatsWrapper) Swap(i, j int) {
    sw.stats[i], sw.stats[j] = sw.stats[j], sw.stats[i]
}
func (sw StatsWrapper) Less(i, j int) bool {
    return sw.by(sw.stats[i], sw.stats[j])
}
 

func SortStats(stats []*Stats, by SortBy) {  
    sort.Sort(StatsWrapper{stats, by})
}

