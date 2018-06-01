#!/bin/bash

PWD=$(pwd)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARAMS=$@

for i in 4.9 5.4 5.5 6.4 7.3 clang_3.5 clang_3.8
 do docker run -it -v $DIR/../:/simplx -u $(id -u):$(id -g) --rm volatilebitfield/cpp:$i bash -c "rm -rf /simplx/build; mkdir /simplx/build ;cd /simplx/build/ ; cmake $@ ..; make -j8; make test";
done;
