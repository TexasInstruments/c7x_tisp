#!/bin/bash

BUILD_TYPE=${1}

echo "BUILD_TYPE: ${BUILD_TYPE}"

rm -rf bin

if [[ "${BUILD_TYPE}" == "PC" ]]; then

   rm -rf build

   cmake -B ~/TISP/build -DSOC=AM62A -DDEVICE=C7504 -DBUILD_TEST=1 -DKERNEL_NAME=TISP_add -DTARGET_PLATFORM=PC -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DCMAKE_RULE_MESSAGES:BOOL=OFF -DDEMO_CASE=1

   cmake --build ~/TISP/build -j$(nproc)

   rm -rf build
   cmake -B ~/TISP/build -DSOC=AM62A -DDEVICE=C7504 -DBUILD_TEST=1 -DKERNEL_NAME=TISP_addConstant -DTARGET_PLATFORM=PC -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DCMAKE_RULE_MESSAGES:BOOL=OFF -DDEMO_CASE=1

   cmake --build ~/TISP/build -j$(nproc)

   bin/Release/test_TISP_add_C7504_x86_64

   bin/Release/test_TISP_addConstant_C7504_x86_64
else   

    rm -rf build

    cmake -B ~/TISP/build -DSOC=AM62A -DDEVICE=C7504 -DBUILD_TEST=1 -DKERNEL_NAME=TISP_add -DTARGET_PLATFORM= -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DCMAKE_RULE_MESSAGES:BOOL=OFF -DDEMO_CASE=1

    cmake --build ~/TISP/build -j$(nproc)

    rm -rf build

    cmake -B ~/TISP/build -DSOC=AM62A -DDEVICE=C7504 -DBUILD_TEST=1 -DKERNEL_NAME=TISP_addConstant -DTARGET_PLATFORM= -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DCMAKE_RULE_MESSAGES:BOOL=OFF -DDEMO_CASE=1

    cmake --build ~/TISP/build -j$(nproc)
fi
