#!/bin/bash
# build file for OmniOperatorJit
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.

set -e

source $(cd $(dirname ${BASH_SOURCE[0]}) && pwd)/env_check.sh

TARGZ_NAME=boostkit-omniop-operator-2.2.0-aarch64
ZIP_NAME=BoostKit-omniop_2.2.0

# if either help or --help is provided, the usage should be printed prior to exit
if [ "$1" = 'help' ] || [ "$1" = '--help' ]; then
  print_usage
  exit 0
fi

# check if required env vars are set
check_java_home
check_omni_home
check_ninja

### main build begins here ###
echo "Start building modules using $1"
echo "-- Enter" $(dirname $(readlink -f $0))

# save working directory
CWD=$(pwd)
# check for $1 param
case "$1" in
  package)
    setup_dependencies package

    echo "-- Package without test"
    cd ${CWD} && build release:java --exclude-test

    cd $CWD/bindings/java && mvn clean install -Domni.home=$OMNI_HOME -DskipTests
    cd $CWD/core/src/udf/java && mvn clean install -DskipTests

    cd $CWD
    # clean environment
    [ -d "$TARGZ_NAME" ] && rm -rf $TARGZ_NAME
    [ -f "$TARGZ_NAME.tar.gz" ] && rm -rf $TARGZ_NAME.tar.gz
    [ -f "$ZIP_NAME.zip" ] && rm -rf $ZIP_NAME.zip

    cp -r $OMNI_HOME/lib $TARGZ_NAME
    cp $CWD/bindings/java/target/*-aarch64.jar $TARGZ_NAME
    cp $CWD/core/src/udf/java/target/*-aarch64.jar $TARGZ_NAME
    tar --owner root --group root -zcvf $TARGZ_NAME.tar.gz $TARGZ_NAME
    zip $ZIP_NAME.zip $TARGZ_NAME.tar.gz
    ;;
  release)
    setup_dependencies release

    echo "-- Only build"
    cd ${CWD} && build release:java --exclude-test 

    cd $CWD/bindings/java && mvn clean install -Domni.home=$OMNI_HOME -DskipTests
    cd $CWD/core/src/udf/java && mvn clean install -DskipTests
    ;;
  test)
    setup_dependencies

    echo "-- Enable build and test"
    cd ${CWD} && build release:java 
    $CWD/build/core/test/omtest --gtest_output=xml:test_detail.xml

    cd $CWD/bindings/java && mvn clean install -Domni.home=$OMNI_HOME
    cd $CWD/core/src/udf/java && mvn clean install
    ;;
  coverage-java)
    setup_dependencies release
    echo "-- Enable coverage for java"
    cd ${CWD} && build release:java 

    cd $CWD/bindings/java && mvn clean install devtestcov:atest -Domni.home=$OMNI_HOME -Dactive.devtest=true -Dmaven.test.failure.ignore=true -Djacoco-agent.destfile=target/jacoco.exec -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true
    cd $CWD/core/src/udf/java && mvn clean install
    ;;
  coverage-c++)
    export CCACHE_LOGFILE=ccache.log
    export CCACHE_DEBUG=1

    setup_dependencies release
    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- Enable coverage for c++"
    cd ${CWD} && build coverage:java
    
    # 清理历史覆盖率文件
    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- Clean old coverage files"
    find ${CWD}/build -name "*.gcda" -delete

    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- omtest --gtest_output=xm"
    $CWD/build/core/test/omtest --gtest_output=xml:${CWD}/core/build/test_detail.xml

    # 使用fastcov
    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- fastcov collecting..."
    fastcov \
      -d ${CWD}/build/core \
      -o test.info \
      --lcov \
      --include "${CWD}/core/" \
      --exclude "/usr/include/" \
      --exclude "$CWD/build/core/open_source/" \
      --branch-coverage \
      -j 16
  
    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- lcov generate html"
    touch final.info
    genhtml test.info -o ${CWD}/core/build/test_coverage --branch-coverage --rc lcov_branch_coverage=1
    echo "[$(date +'%Y-%m-%d %H:%M:%S')]-- Finash coverage for c++"
    ;;
  coverage)
    setup_dependencies package

    echo "-- Package asan without test"
    cd ${CWD} && build coverage:java --exclude-test 

    cd $CWD/bindings/java && mvn clean install -Domni.home=$OMNI_HOME -DskipTests
    cd $CWD/core/src/udf/java && mvn clean install -DskipTests

    cd $CWD
    # clean environment
    [ -d "$TARGZ_NAME" ] && rm -rf $TARGZ_NAME
    [ -f "$TARGZ_NAME.tar.gz" ] && rm -rf $TARGZ_NAME.tar.gz
    [ -f "$ZIP_NAME.zip" ] && rm -rf $ZIP_NAME.zip

    cp -r $OMNI_HOME/lib $TARGZ_NAME
    cp $CWD/bindings/java/target/*-aarch64.jar $TARGZ_NAME
    cp $CWD/core/src/udf/java/target/*-aarch64.jar $TARGZ_NAME
    tar --owner root --group root -zcvf $TARGZ_NAME.tar.gz $TARGZ_NAME
    zip $ZIP_NAME.zip $TARGZ_NAME.tar.gz
    ;;
  *)
    echo "-- Enable default options"
    cd ${CWD} && build release:java 
    $CWD/build/core/test/omtest --gtest_output=xml:${CWD}/core/build/test_detail.xml

    cd $CWD/bindings/java && mvn clean -Domni.home=$OMNI_HOME install
    cd $CWD/core/src/udf/java && mvn clean install
    ;;
esac
