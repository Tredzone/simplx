#!/bin/bash

PWD=$(pwd)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


compiler_set_1="4.9 5.4 5.5 6.4 7.3 8.2 clang_3.5 clang_3.8 clang_3.9 clang_4.0"
compiler_set_2="4.9 6.4 8.2 clang_3.8"
compiler_set_3="clang_3.8"


[ "$compiler" == "1" ] && compiler_set="$compiler_set_1"
[ "$compiler" == "2" ] && compiler_set="$compiler_set_2"
[ "$compiler" == "3" ] && compiler_set="$compiler_set_3"
[ "$compiler" == "" ] && compiler_set="$compiler_set_1"

[ "$dorelease" == "" ] && dorelease="1"
[ "$dodebug" == "" ] && dodebug="1"

[ "$dotestengine" == "" ] && dotestengine="1"
[ "$dotestconnector" == "" ] && dotestconnector="1"
[ "$dotutorials" == "" ] && dotutorials="1"

function test
{
tmpfile=$(mktemp)

for i in $compiler_set
do

# unitary tests engine
 [ "$dotestengine" == "1" ] && docker run -it -v $DIR/../:/simplx -u $(id -u):$(id -g) --rm volatilebitfield/cpp:$i bash -c " ! ( rm -rf /simplx/test/build && mkdir /simplx/test/build && cd /simplx/test/build/ &&  cmake $* .. && make -j8 && make test ) && echo [DEADBEEF] FAILED [$i]" | tee $tmpfile ; grep "DEADBEEF" $tmpfile > /dev/null && exit

# unitary tests connector tcp
 [ "$dotestconnector" == "1" ] && docker run -it -v $DIR/../:/simplx -u $(id -u):$(id -g) --rm volatilebitfield/cpp:$i bash -c " ! ( rm -rf /simplx/test/connector/tcp/build && mkdir /simplx/test/connector/tcp/build && cd /simplx/test/connector/tcp/build/ &&  cmake $* .. && make -j8 && ./clientservertestu.bin && make test )  && echo [DEADBEEF] FAILED [$i]" | tee $tmpfile ; grep "DEADBEEF" $tmpfile > /dev/null && exit

# tutorial
 [ "$dotutorial" == "1" ] && docker run -it -v $DIR/../:/simplx -u $(id -u):$(id -g) --rm volatilebitfield/cpp:$i bash -c " ! ( cd /simplx/tutorial && find ./ -maxdepth 1 -iname \"??_*\" -exec bash -c \"f={} && cd \\\$f && rm -rf build && mkdir build && cd build && cmake .. && make -j8\" \; ) && echo [DEADBEEF] FAILED [$i]" | tee $tmpfile ; grep "DEADBEEF" $tmpfile > /dev/null && exit

done;
rm -rf $tmpfile
}

CMAKE_BUILD_TYPE_str="CMAKE_BUILD_TYPE"
if [[ "$@" == *"$CMAKE_BUILD_TYPE_str"* ]];
then
test "$@"
else
 [ "$dodebug" == "1" ] && test -DCMAKE_BUILD_TYPE=DEBUG "$@" 
 [ "$dorelease" == "1" ] && test -DCMAKE_BUILD_TYPE=RELEASE "$@"
fi
