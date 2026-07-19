#!/usr/bin/env bash

make -j$(nproc) libdrm-rebuild testapp-rebuild all
