#!/bin/bash
export ONL_ARCH="amd64"
dpkg-buildpackage -b -us -uc -a"$ONL_ARCH"
