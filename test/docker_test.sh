#!/bin/bash

PWD=$(pwd)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

tmpfile=$(mktemp)
for i in 4.9 5.4 5.5 6.4 7.3 8.2 clang_3.5 clang_3.8
 do docker run -it -v $DIR/../:/simplx -u $(id -u):$(id -g) --rm volatilebitfield/cpp:$i bash -c "rm -rf /simplx/build; mkdir /simplx/build ;cd /simplx/build/ ; ! ( cmake -DBUILD_TEST=1 .. && make -j8 && make test ) && echo [DEADBEEF] FAILED [$i]" | tee $tmpfile
 grep "DEADBEEF" $tmpfile >/dev/null && break
done;
rm -rf $tmpfile
