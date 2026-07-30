#!/bin/bash
# check env for OmniOperatorJit
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.

set -e

print_gcc_lib() {
  gcc -print-search-dirs | sed '/^lib/b 1;d;:1;s,/[^/.][^/]*/\.\./,/,;t 1;s,:[^=]*=,:;,;s,;,;  ,g' | tr \; \\012
}

# print help manual
print_help() {
  echo "
  Usage:
  build.sh [type:binding-target] [options]

  Binding Targets:
    java                               = Java binding library

  Types:
    debug                              = Enable Debug
    release                            = Enable Release
    trace                              = Enable Trace
    coverage                           = Enable Coverage

  Options:
    all                                = Enable All Module Debug, Include: OPERATOR,VECTOR,LLVM
    op,   --enable-operator-debug      = Enable Operator Debug
    vec,  --enable-vector-debug        = Enable Vector Debug
    llvm, --enable-llvm-debug          = Enable LLVM Debug
    --disable-cpuchecker               = Disable CPU checker
    --enable-dt                        = Enable DT checker
    --exclude-test                     = Exclude Test Source
  "
}

# if JAVA_HOME is not set,
# prompt to set to detected location before exiting
check_java_home() {
  if [ -z "$JAVA_HOME" ]; then
    local java=$(which java|xargs readlink -f)

    echo "ERROR: JAVA_HOME is not set!"
    echo "If it's ${java%/bin/java},"
    echo "you can set it as follows:"
    echo "export JAVA_HOME=${java%/bin/java}"
    echo ""
    echo "Please set JAVA_HOME and try again"

    exit 1
  fi
}

# if OMNI_HOME is not set,
# prompt to set to suggested location before exiting
check_omni_home() {
  if [ -z "$OMNI_HOME" ]; then
    echo "ERROR: OMNI_HOME is not set!"
    echo "You can set it as follows:"
    echo "for system level configuration (requiring root privilege),"
    echo "export OMNI_HOME=/opt"
    echo "or for user level configuration,"
    echo "export OMNI_HOME=$HOME/opt"
    echo ""
    echo "Please set OMNI_HOME and try again"

    exit 1
  fi
}

check_set_prerequisites() {
  check_java_home
  check_omni_home
  check_ninja

  echo "OMNI_HOME = $OMNI_HOME"
  LIB_HOME=$OMNI_HOME/lib
  [ ! -d "$LIB_HOME" ] && mkdir -p $LIB_HOME

  echo "LIB_HOME = $LIB_HOME, LD_LIBRARY_PATH = $LD_LIBRARY_PATH"

  rm -rf $LIB_HOME/libboostkit*.so $OMNI_HOME/*-binding
}

exit_with_message_and_print_help()
{
   echo ""
   echo "$1"
   print_help
   exit 1
}

check_ninja() {
  if ! command -v ninja &> /dev/null; then
    echo "ERROR: Ninja is not installed!"
    echo "Ninja is required for optimized build performance."
    echo "Please install Ninja and try again."
    exit 1
  else
    echo "Ninja version: $(ninja --version)"
  fi
}

exit_with_message()
{
   echo ""
   echo "$1"
   exit 1
}