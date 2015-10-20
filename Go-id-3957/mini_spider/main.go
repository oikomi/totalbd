package main

import (
    "gopkg.in/gcfg.v1"
)

import (
    "Go-id-3957/mini_spider/conf"
)

func main() {
    var cfg conf.Config
    err := gcfg.ReadFileInto(&cfg, "myconfig.gcfg")

    if err != nil {

    }
}
