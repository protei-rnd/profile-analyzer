#!/usr/bin/env bash

cd 3dparty

rm -rf libunwind xz
git clone https://git.tukaani.org/xz.git
git clone git://git.sv.gnu.org/libunwind.git
