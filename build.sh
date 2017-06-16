#!/usr/bin/env bash

build_3dparty_lib() {
    if [ $? -eq 0 ]; then
        echo -ne "\n\nstart building $1\n\n" && \
        rm -rf build && mkdir build && ./autogen.sh && ./configure --prefix=`pwd`/build && \
        make -j4 -l5 && echo -ne "\n\n$1 was built\n\n" && \
        make install && echo -ne "\n\n$1 was installed\n\n" 

        if [ $? -ne 0 ]; then
            echo -ne "fail built $1"
            exit
        fi

        cd ../../
    fi
}

build_project() {
    if [ $? -eq 0 ]; then
        rm -rf build && mkdir build && cd build && echo -ne "\n\nstart building $1\n\n" && \
        cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j4 -l5 && cd ../ && echo -ne "\n\n$1 was built\n\n" 

        if [ $? -ne 0 ]; then
            echo -ne "fail built $1"
            exit
        fi
    fi
}

if [ -z ${QMAKE_PATH+x} ]; then QMAKE_PATH=qmake; fi
echo -ne "\n\npath to qmake: $QMAKE_PATH\n\n"

cd 3dparty/psc
build_project "psc"
cd ../../

cd 3dparty/xz 
build_3dparty_lib "liblzma"

cd 3dparty/libunwind 
build_3dparty_lib "libunwind"

cd profiler 
build_project "profile analyzer"
cd ../

cd gui && rm -rf build && mkdir build && cd build && echo -ne "\n\nstart building gui\n\n" && \
$QMAKE_PATH ../QtTree.pro && make -j4 -l5 && cd ../../ && echo -ne "\n\nsuccess\n\n"

[ $? -ne 0 ] && echo -ne "\n\nfail\n\n"

