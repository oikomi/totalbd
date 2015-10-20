#! /bin/bash

pkill baidu-daemon
./baidu-daemon > /dev/null 2>&1  &
