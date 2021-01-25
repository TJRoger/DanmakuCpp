#!/bin/bash
# coding: utf-8
###########################################################################
# Copyright © 2021 Roger Luo. All rights reserved.
###########################################################################
# Author: Roger Luo[tjrogertj@gmail.com]
# Created At: 2021-01-26 02:24:52
# File: build.sh
# desc:
# vim: set ts=4 sw=4 sts=4 tw=100
cmake \
    -H. \
    -B./build \
    -DCMAKE_BUILD_TYPE=Release \
    -DJSONCPP_WITH_CMAKE_PACKAGE=ON
