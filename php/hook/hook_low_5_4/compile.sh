#! /bin/bash

make clean

phpize

./configure  --without-mysql  --with-debugfile=/tmp/apm/baidu.log

#./configure   --with-debugfile=/tmp/baidu.log

make && make install

pkill php-fpm

/mh/phphook/php/sbin/php-fpm -R

