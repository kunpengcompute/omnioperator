#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
# OmniOperator is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan
# PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e

if [ -z "$OMNI_HOME" ]
then
   echo "[ERROR] can not found environment variable OMNI_HOME." >&2
   exit 1
fi

test -d $OMNI_HOME/java-binding && JAVA_BINDING=$(find $OMNI_HOME/java-binding -name '*.so')

build_params=($1)

if [ ! -z "${build_params}" ] && [ "${build_params}" != 'NONE' ]
then
  echo "[INFO] build omni library."
  export LIBRARY_PATH=$OMNI_HOME/lib:$LIBRARY_PATH
  export LD_LIBRARY_PATH=$OMNI_HOME/lib:$LD_LIBRARY_PATH

  build_type=$(echo "${build_params[0]}" | awk -F ':' '{print $1}')
  unset build_params[0]

  . build_scripts/build.sh ${build_type}:java ${build_params[@]}
else
  if [ -z "$JAVA_BINDING" ]
  then
      echo "[INFO] no valid lib found in OMNI_HOME." >&2
  else
      echo "[INFO] using $(basename $JAVA_BINDING) in OMNI_HOME"
  fi
fi
