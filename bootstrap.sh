#!/usr/bin/env bash
set -ex

sudo apt-get update
sudo apt-get -y install git wget make libncurses-dev flex bison gperf python2.7 python-serial g++
sudo apt-get -y install gawk gperf grep gettext libncurses-dev python python-dev automake bison flex texinfo help2man libtool unzip
