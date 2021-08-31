#!/usr/bin/make -f

SETTINGS = ARCH=amd64 CONFIG=Debug

all:
	make -C scripts -f sidury.mak $(SETTINGS)
	make -C scripts -f client.mak $(SETTINGS)
	make -C chocolate/scripts -f chocolate.mak $(SETTINGS)

