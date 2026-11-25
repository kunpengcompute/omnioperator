#!/bin/bash
# Project license notice for OmniOperator
# Existing file-specific copyright and license terms continue to apply.
#
# OmniOperator is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan
# PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# This script is used to compile the omnioperator from the source code, but it is not the full source code of omnioperator.
set -ex
SCRIPT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SOURCE_ROOT=$(cd "${SCRIPT_ROOT}/../.." && pwd)
# ====================== HELP 函数 ======================
usage() {
cat <<EOF
Usage:
  bash OmniOperator_build.sh [compile|package]

可覆盖参数（未传参时读 build.ini）：
  jdk_version             JDK版本，可选 8 / 17
  OmniOperatorJIT_Version OmniOperatorJIT版本号，用于打包命名
  OMNI_COMPILER_THREAD_COUNT 编译线程数

Functions:
  compile        只编译，不打包
  package        编译源码 + 生成zip软件包、sha256sum、软件信息文件
  coverages_cpp  编译并跑 C++ 覆盖率
  debug          编译覆盖率版本后打包

Example run:
# 门禁：全部使用 build.ini 默认值
bash OmniOperator_build.sh compile

# 版本构建：入参覆盖
export jdk_version="17" OmniOperatorJIT_Version="2.3.0"
bash OmniOperator_build.sh package

Options:
  -h | --help        Show this help message and exit
EOF
}

# 帮助参数解析，放脚本最前面
if [[ $# -ge 1 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
    usage
    exit 0
fi

# 环境变量校验函数
check_required_env() {
    source "${SCRIPT_ROOT}/init_env.sh"
    print_config
    require_config OmniOperatorJIT_Version jdk_version OMNI_COMPILER_THREAD_COUNT
}

# 校验入参
check_args() {
    local arg="$1"
    case "${arg}" in
        compile | package | coverages_cpp | debug) ;;
        *)
            echo "ERROR: unsupported parameter '${arg}'" >&2
            echo "Valid parameter: compile / package / coverages_cpp / debug" >&2
            echo "Use -h for help" >&2
            exit 1
            ;;
    esac
}

function jdk17_install(){
    export JAVA_HOME=/opt/buildtools/bisheng-jdk-17.0.18
}

function jdk8_set(){
    export JAVA_HOME=/opt/buildtools/bisheng-jdk1.8.0_342
    export JRE_HOME=/opt/buildtools/bisheng-jdk1.8.0_342/jre
    export PATH=$JRE_HOME/bin:$PATH
    export CLASSPATH=${JRE_HOME}/lib:$CLASSPATH
}
function config_env(){
    if [ "${jdk_version}" = "17" ];then
        jdk17_install
    else
        jdk8_set
        export jdk_version="8"
    fi
    export MAVEN_HOME=/opt/buildtools/apache-maven-3.9.9
    export MAVEN_OPTS="${MAVEN_OPTS:-} -Dmaven.repo.local=/home/jenkins/agent/build_cache/.maven_cache"
    export PROTOBUF_HOME=/opt/buildtools/Protobuf-3.21.9
    export CMAKE_ROOT=/opt/buildtools/cmake-3.28.2-linux-aarch64/share
    export PATH=${MAVEN_HOME}/bin:$HOME/.local/bin:${PROTOBUF_HOME}/bin:${JAVA_HOME}/bin:$PATH
    export CLASSPATH=${JAVA_HOME}/lib:$CLASSPATH
    export CMAKE_PREFIX_PATH=$PROTOBUF_HOME
    export Protobuf_ROOT=$PROTOBUF_HOME
    export Protobuf_PROTOC_EXECUTABLE=$PROTOBUF_HOME/bin/protoc
    export LD_LIBRARY_PATH=/opt/buildtools/libboundscheck/lib:$PROTOBUF_HOME/lib:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/opt/buildtools/libboundscheck/lib:$PROTOBUF_HOME/lib:/usr/local/lib:/usr/local/lib64:$LIBRARY_PATH
    export C_INCLUDE_PATH=/opt/buildtools/libboundscheck/include:/opt/buildtools/rapidjson/include:/opt/buildtools:$PROTOBUF_HOME/include:$JAVA_HOME/include:/include:/usr/local/include:$C_INCLUDE_PATH
    export CPLUS_INCLUDE_PATH=/opt/buildtools/libboundscheck/include:/opt/buildtools/rapidjson/include:/opt/buildtools:$PROTOBUF_HOME/include:$JAVA_HOME/include:/usr/local/include:$CPLUS_INCLUDE_PATH
    [ -n "${module}" ] || { echo "ERROR: module 为空，请由 CI 显式传入 module（如 openEuler22.03_JDK8）" >&2; exit 1; }
    export OMNI_HOME="${SOURCE_ROOT}"
    mkdir -p "${OMNI_HOME}/lib"
    agentpath="${WORKSPACE}/output/${module}"
    if [ -d "${agentpath}" ];then rm -rf ${agentpath}; fi
    mkdir -p ${agentpath}
}

function compile_operator(){
    config_env
    pushd "${SOURCE_ROOT}"
    sh build.sh package
}

function build_OmniOperatorJIT_compile_debug() {
    config_env
    echo "Build start build_OmniOperatorJIT"
    pushd "${SOURCE_ROOT}"
    sh build.sh coverage
}

function build_OmniOperatorJIT_cpp_coverage() {
    config_env
    pushd "${SOURCE_ROOT}"
    sh build.sh coverage-c++
}

function BoostKit_OmniOperatorJIT_Version_info(){
    touch version.txt
    echo "Product Name: Kunpeng BoostKit" >> version.txt
    echo "Product Version: ${Product_Version}" >> version.txt
    echo "Component Name: BoostKit-omniop" >> version.txt
    echo "Component Version: ${OmniOperatorJIT_Version}" >> version.txt
}

function clone_sourcecode_rely(){
    git clone --depth 1 --branch v1.1.16 https://gitcode.com/openeuler/libboundscheck.git
    git clone --depth 1 --branch v1.1.0 https://gitcode.com/GitHub_Trending/ra/rapidjson.git
    git clone --depth 1 --branch v3.11.3 https://gitee.com/mirrors/nlohmann-json.git json
}

function do_package(){
    os_version=$(grep PRETTY_NAME /etc/os-release | cut -d= -f2 | sed -E 's/"([A-Za-z0-9]+) ([0-9]+\.[0-9]+).*/\1\2/')
    OS_type=${os_version}
    if [ -d "${module}" ];then rm -rf ${module}; fi && mkdir -p ${module}
    cp -rf boostkit-omniop-operator-${OmniOperatorJIT_Version}-aarch64.tar.gz ${module}/boostkit-omniop-operator-${OmniOperatorJIT_Version}-aarch64-${OS_type}.tar.gz
    if [ -f "version.txt" ];then rm -rf version.txt; fi && BoostKit_OmniOperatorJIT_Version_info
    zip -r BoostKit-omniruntime-omnioperator-${OmniOperatorJIT_Version}.zip ${module} version.txt
    cp BoostKit-omniruntime-omnioperator-${OmniOperatorJIT_Version}.zip ${agentpath}/
    pushd ${agentpath}/
    sha256sum BoostKit-omniruntime-omnioperator-${OmniOperatorJIT_Version}.zip > BoostKit-omniruntime-omnioperator-${OmniOperatorJIT_Version}.zip.sha256sum
    python3 "${SCRIPT_ROOT}/collect_software_info.py" "${WORKSPACE}" BoostKit-omniruntime-omnioperator-${OmniOperatorJIT_Version}.zip
    popd
}
function OmniOperator_build(){
    parameter="$1"
    case ${parameter} in
        "compile")
            clone_sourcecode_rely
            compile_operator
            ;;
        "package")
            clone_sourcecode_rely
            compile_operator
            do_package
            ;;
        "coverages_cpp")
            clone_sourcecode_rely
            build_OmniOperatorJIT_cpp_coverage
            ;;
        "debug")
            clone_sourcecode_rely
            build_OmniOperatorJIT_compile_debug
            do_package
            ;;
        *)
            echo "ERROR: unsupported parameter '${parameter:-<empty>}'" >&2
            usage
            exit 1
            ;;
    esac
}

# 主入口
main(){
    local action="$1"
    check_args "${action}"
    check_required_env
    set_ccache
    ccache -z || true
    OmniOperator_build "${action}"
    ccache -s || true
}

main "$@"
