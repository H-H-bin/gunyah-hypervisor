# coding: utf-8
#
# Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause

import sys


keepmodules = set(sys.modules.keys())
import configure as configure

Import('env', 'build_dir', 'gunyah_config_args', 'gunyah_build_args')

build = configure.SConsBuild(env, Builder, Action, build_dir=str(build_dir),
                             arguments=gunyah_config_args)
targets = build(**gunyah_build_args)

for n in set(sys.modules.keys()) - keepmodules:
    del sys.modules[n]
Return('targets')
