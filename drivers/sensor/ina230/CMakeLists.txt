#
# Copyright (c) 2020-2020 Framework COmputer LLC
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_INA230 ina230.c)
