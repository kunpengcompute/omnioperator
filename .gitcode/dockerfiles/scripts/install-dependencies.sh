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

set -euox pipefail
readonly BUILD_PARALLELISM="${BUILD_PARALLELISM:-32}"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
source "${SCRIPT_DIR}/install-buildtools.sh"

readonly -a supported_tools=(
    bisheng-jdk-17.0.18
    bisheng-jdk-8u342
    cmake-3.28.2
    maven-3.9.9
    llvm-15.0.4
    lcov
)

readonly -a supported_libs=(
    libboundscheck-1.1.16
    googletest-1.12.0
    jemalloc-5.3.0
    nlohmann-json-3.11.3
    snappy-1.1.10
    abseil-cpp-20250127.0
    re2-2024-07-02
    rapidjson-1.1.0
    protobuf-3.21.9
    zlib-1.3.1
    zlib-ng-2.0.4
    lz4-1.10.0
    zstd-1.5.6
    cyrus-sasl-2.1.28
    jsoncpp-1.9.6
    arrow-11.0.0
    orc-1.7.4
    hadoop-3.2.0
    fmt-10.1.1
    folly-2024.07.01.00
)

readonly -a supported_install=(
    "${supported_tools[@]}"
    "${supported_libs[@]}"
)
readonly -a supported_setup=(maven)

readonly -a base_pkgs=(
    autoconf boost-devel cmake curl dos2unix file findutils gcc gcc-c++ gdb git
    glibc-locale-source iproute iputils make maven net-tools ninja-build ccache
    openssh-clients openssh-server openssl passwd patch perf sudo unzip vim wget
    xz zip zlib pcre2 rpm-build libasan libasan-static jq python3-pip
    double-conversion-devel libevent-devel glog-devel
    binutils binutils-devel tree doxygen patchelf texinfo automake libtool openssl-devel google-benchmark-devel
)

usage() {
    cat <<'EOF'
Usage:
  bash scripts/install-dependencies.sh --install <component>
  bash scripts/install-dependencies.sh --install-base-pkgs
  bash scripts/install-dependencies.sh --install-all-tools
  bash scripts/install-dependencies.sh --install-all-libs
  bash scripts/install-dependencies.sh --setup <type>
  bash scripts/install-dependencies.sh --help

Options:
  --install <component>              Install one component
  --install-base-pkgs               Install the base OS packages required to build
  --install-all-tools               Install all build tools (jdk/cmake/maven/llvm)
  --install-all-libs                Install all third‑party libraries(exclude boostkit)
  --setup <type>                    Apply one configuration type
  -h, --help                        Show this help message

Supported build‑tool targets:
EOF
    printf '  %s\n' "${supported_tools[@]}"
    printf '\nSupported library targets(non‑boostkit):\n'
    printf '  %s\n' "${supported_libs[@]}"
    printf '\nSupported boostkit/omni targets:\n'
    printf '  %s\n' "${supported_setup[@]}"
}

is_supported() {
    local target="$1"
    shift
    local supported
    for supported in "$@"; do
        if [[ "$target" == "$supported" ]]; then
            return 0
        fi
    done
    return 1
}

install_base_pkgs() {
    echo -e "\n===== Install base packages ====="
    yum install -y --setopt=install_weak_deps=False "${base_pkgs[@]}"
    pip3 install --no-cache-dir pandas tqdm openpyxl pyinstaller fastcov pre-commit \
        -i http://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com
    yum clean all
    rm -rf /var/cache/dnf /var/cache/yum /root/.cache/pip
}

install_all_tools() {
    for comp in "${supported_tools[@]}"; do
        echo -e "\n===== Install tool: ${comp} ====="
        "install_${comp//[.-]/_}"
    done
}

install_all_libs() {
    for comp in "${supported_libs[@]}"; do
        echo -e "\n===== Install library: ${comp} ====="
        "install_${comp//[.-]/_}"
    done
    # 留一份体积清单在构建日志里，下次瘦身有据可查
    du -sh /opt/buildtools /usr/local 2>/dev/null | sed 's/^/du: /' || true
}

install_googletest_1_12_0() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch release-1.12.0 https://gitee.com/mirrors/googletest.git
    cd /opt/buildtools/googletest
    mkdir -p /opt/buildtools/googletest/build && cd /opt/buildtools/googletest/build
    cmake ..
    make -j"$BUILD_PARALLELISM"
    make install
    rm -rf /opt/buildtools/googletest
}

install_jemalloc_5_3_0() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 5.3.0 https://gitee.com/mirrors/jemalloc.git
    cd /opt/buildtools/jemalloc
    ./autogen.sh --disable-initial-exec-tls --with-lg-page=16
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/jemalloc
}

install_nlohmann_json_3_11_3() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v3.11.3 https://gitee.com/mirrors/nlohmann-json.git json
    mkdir -p /opt/buildtools/json/build && cd /opt/buildtools/json/build
    cmake ..
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/json
}

install_snappy_1_1_10() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch openEuler-24.03-LTS-SP1-release https://gitee.com/src-openeuler/snappy.git
    cd /opt/buildtools/snappy
    tar -zxvf snappy-1.1.10.tar.gz
    cd snappy-1.1.10 && patch -p1 < ../add-option-to-enable-rtti-set-default-to-current-ben.patch && patch -p1 < ../remove-dependency-on-google-benchmark-and-gmock.patch
    mkdir -p /opt/buildtools/snappy/snappy-1.1.10/build && cd /opt/buildtools/snappy/snappy-1.1.10/build
    cmake -DSNAPPY_BUILD_BENCHMARKS=OFF -DSNAPPY_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON  ..
    make -j"$BUILD_PARALLELISM" && make install
    echo 'export LIBRARY_PATH=/usr/local/lib64:$LIBRARY_PATH' >> /etc/profile
    echo 'export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH' >> /etc/profile
    rm -rf /opt/buildtools/snappy
}

install_libboundscheck_1_1_16() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v1.1.16 https://gitcode.com/openeuler/libboundscheck.git
    cd /opt/buildtools/libboundscheck
    make CC=gcc
    echo 'export C_INCLUDE_PATH=/opt/buildtools:/opt/buildtools/libboundscheck/include:$C_INCLUDE_PATH' >> /etc/profile
    echo 'export CPLUS_INCLUDE_PATH=/opt/buildtools:/opt/buildtools/libboundscheck/include:$CPLUS_INCLUDE_PATH' >> /etc/profile
    echo 'export LIBRARY_PATH=/opt/buildtools/libboundscheck/lib:$LIBRARY_PATH' >> /etc/profile
    echo 'export LD_LIBRARY_PATH=/opt/buildtools/libboundscheck/lib:$LD_LIBRARY_PATH' >> /etc/profile
    # include/ 与 lib/ 被 /etc/profile 和 init_env 之后的构建脚本直接引用，其余只增体积
    find /opt/buildtools/libboundscheck -mindepth 1 -maxdepth 1 ! -name include ! -name lib -exec rm -rf {} +
}

install_abseil_cpp_20250127_0() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 20250127.0 https://gitee.com/mirrors/abseil-cpp.git
    mkdir -p /opt/buildtools/abseil-cpp/build && cd /opt/buildtools/abseil-cpp/build
    cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DABSL_PROPAGATE_CXX_STD=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    make -j"$BUILD_PARALLELISM" install
    rm -rf /opt/buildtools/abseil-cpp
}

install_re2_2024_07_02() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 2024-07-02 https://gitee.com/mirrors/re2.git
    mkdir -p /opt/buildtools/re2/build && cd /opt/buildtools/re2/build
    cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_FLAGS="-fPIC"
    make -j"$BUILD_PARALLELISM" install
    rm -rf /opt/buildtools/re2
}

install_rapidjson_1_1_0() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v1.1.0 https://gitcode.com/GitHub_Trending/ra/rapidjson.git
    echo 'export C_INCLUDE_PATH=/opt/buildtools/rapidjson/include:$C_INCLUDE_PATH' >> /etc/profile
    echo 'export CPLUS_INCLUDE_PATH=/opt/buildtools/rapidjson/include:$CPLUS_INCLUDE_PATH' >> /etc/profile
    # header-only，profile 只引用 include/，历史与测试/文档/example 全丢
    find /opt/buildtools/rapidjson -mindepth 1 -maxdepth 1 ! -name include -exec rm -rf {} +
}

readonly OPERATOR_PROTOBUF_HOME="${OPERATOR_PROTOBUF_HOME:-/opt/buildtools/Protobuf-3.21.9}"
readonly BOOSTKIT_BIGDATA_URL="https://gitee.com/kunpengcompute/boostkit-bigdata.git"

# arrow 与 hadoop 的补丁都取自 boostkit-bigdata；补丁拷走即删，不留 GB 级 clone
operator_contrib_dir() {
    if [[ ! -d /opt/buildtools/boostkit-bigdata ]]; then
        mkdir -p /opt/buildtools
        cd /opt/buildtools
        git clone --depth 1 -b main "${BOOSTKIT_BIGDATA_URL}" /opt/buildtools/boostkit-bigdata || exit 1
    fi
    printf '%s' /opt/buildtools/boostkit-bigdata
}

install_protobuf_3_21_9() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 https://github.com/protocolbuffers/protobuf.git
    cd protobuf
    git fetch --depth 1 origin tag v3.21.9
    git checkout v3.21.9
    ./autogen.sh
    ./configure --enable-static=no --prefix="${OPERATOR_PROTOBUF_HOME}"
    make -j"$BUILD_PARALLELISM" && make install
    echo "export PROTOBUF_HOME=${OPERATOR_PROTOBUF_HOME}" >> /etc/profile
    echo 'export PATH=$PROTOBUF_HOME/bin:$PATH' >> /etc/profile
    echo 'export LD_LIBRARY_PATH=$PROTOBUF_HOME/lib:/usr/local/lib:$LD_LIBRARY_PATH' >> /etc/profile
    echo 'export LIBRARY_PATH=$PROTOBUF_HOME/lib:/usr/local/lib:$LIBRARY_PATH' >> /etc/profile
    rm -rf /opt/buildtools/protobuf
}

install_zlib_1_3_1() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v1.3.1 https://gitee.com/mirrors/zlib.git
    cd zlib && ./configure
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/zlib
}

# zlib-ng 装成静态 + PIC，arrow/orc 静态链接时用它顶替 zlib（-lz-ng）
install_zlib_ng_2_0_4() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 2.0.4 https://github.com/zlib-ng/zlib-ng.git
    cd zlib-ng && ./configure && mkdir -p build && cd build
    cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON ../
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/zlib-ng
}

install_lz4_1_10_0() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v1.10.0 https://gitee.com/mirrors/LZ4_old1.git lz4
    cd lz4
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/lz4
}

install_zstd_1_5_6() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch v1.5.6 https://gitee.com/mirrors/facebook-zstd.git zstd
    cd zstd
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/zstd
}

install_cyrus_sasl_2_1_28() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch cyrus-sasl-2.1.28 https://gitee.com/mirrors/cyrus-sasl.git
    cd cyrus-sasl
    ./autogen.sh
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/cyrus-sasl
}

# jsoncpp 与 orc 的 cmake 默认要求静态库，保持与原 dockerfile 一致
install_jsoncpp_1_9_6() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 1.9.6 https://gitee.com/mirrors/jsoncpp.git
    cd jsoncpp && mkdir -p build && cd build
    cmake -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF ../
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/jsoncpp
}

install_arrow_11_0_0() {
    local contrib
    contrib=$(operator_contrib_dir)
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 https://gitee.com/mirrors/Apache-Arrow.git arrow
    cd arrow
    git fetch --depth 1 origin tag apache-arrow-11.0.0
    git checkout apache-arrow-11.0.0
    # 11.0.0 的 aarch64 页大小等问题靠 boostkit-bigdata 的补丁解决，补丁版本与 tag 绑定
    cp "${contrib}/omnioperator/contrib/arrow-maint-11_0_0/arrow-maint-11_0_0.patch" .
    git apply arrow-maint-11_0_0.patch
    mkdir -p cpp/_build && cd cpp/_build
    cmake -DARROW_JEMALLOC_LG_PAGE=16 -DARROW_BUILD_STATIC=OFF -DARROW_BUILD_TESTS=OFF \
        -DARROW_ENABLE_TIMING_TESTS=OFF -DARROW_BUILD_INTEGRATION=OFF -DARROW_COMPUTE=ON \
        -DARROW_DATASET=ON -DARROW_EXTRA_ERROR_CONTEXT=ON -DARROW_MIMALLOC=ON \
        -DARROW_SUBSTRAIT=ON -DARROW_WITH_BROTLI=ON -DARROW_WITH_BZ2=ON -DARROW_WITH_LZ4=ON \
        -DARROW_WITH_SNAPPY=ON -DARROW_WITH_UTF8POC=ON -DARROW_WITH_ZLIB=ON -DARROW_WITH_ZSTD=ON \
        -DARROW_HDFS=ON -DCMAKE_BUILD_TYPE=Release -S .. -B .
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/arrow /opt/buildtools/boostkit-bigdata
}

install_orc_1_7_4() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 -b v1.7.4_new \
        https://codehub.devcloud.cn-north-4.huaweicloud.com/b40eab964ee243e1a43336403eba828f/OpenSourceCenter/Adaptor/orc.git
    cd orc && mkdir -p build && cd build
    export ZSTD_HOME=/usr/local LZ4_HOME=/usr/local ZLIB_HOME=/usr/local \
        ZLIB_NG_HOME=/usr/local SNAPPY_HOME=/usr/local \
        PROTOBUF_HOME="${OPERATOR_PROTOBUF_HOME}"
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/usr/local/lib:${OPERATOR_PROTOBUF_HOME}/lib"
    # PROTOBUF_HOME 为空时 ORC 会自建 protobuf 3.5.1，生成的 orc_proto.pb.h 与 3.21.9 头不兼容
    cmake ../ -DBUILD_JAVA=OFF -DANALYZE_JAVA=OFF -DBUILD_LIBHDFSPP=OFF -DBUILD_CPP_TESTS=OFF \
        -DBUILD_TOOLS=ON -DBUILD_POSITION_INDEPENDENT_LIB=ON -DORC_PREFER_STATIC_PROTOBUF=OFF \
        -DCMAKE_PREFIX_PATH="${OPERATOR_PROTOBUF_HOME}" \
        -DPROTOBUF_HOME="${OPERATOR_PROTOBUF_HOME}" \
        -DPROTOBUF_INCLUDE_DIR="${OPERATOR_PROTOBUF_HOME}/include" \
        -DPROTOBUF_LIBRARY="${OPERATOR_PROTOBUF_HOME}/lib/libprotobuf.so" \
        -DPROTOC_LIBRARY="${OPERATOR_PROTOBUF_HOME}/lib/libprotoc.so" \
        -DPROTOBUF_EXECUTABLE="${OPERATOR_PROTOBUF_HOME}/bin/protoc"
    make -j"$BUILD_PARALLELISM" && make install
    gcc -shared -fPIC -o liborc.so -Wl,--whole-archive /usr/local/lib/liborc.a -Wl,--no-whole-archive \
        -L"${OPERATOR_PROTOBUF_HOME}/lib" -L/usr/local/lib -L/usr/local/lib64 \
        -lsasl2 -lssl -lcrypto -lpthread -lprotobuf -lsnappy -lzstd -lz-ng -llz4
    ldd -r liborc.so
    cp -f liborc.so /usr/local/lib/
    ldconfig
    rm -rf /opt/buildtools/orc
}

# libhdfs：hadoop 3.2.0 打 boostkit-bigdata 补丁后只编 native client，头文件与 so 落 /usr/local
install_hadoop_3_2_0() {
    local contrib
    contrib=$(operator_contrib_dir)
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 https://gitee.com/mirrors/hadoop.git
    cd hadoop
    git fetch --depth 1 origin tag rel/release-3.2.0
    git checkout rel/release-3.2.0
    cp "${contrib}/omnioperator/contrib/hadoop-3_2_0/hadoop-3_2_0.patch" .
    git apply hadoop-3_2_0.patch
    cd hadoop-hdfs-project/hadoop-hdfs-native-client/
    # 这两处删行是原 dockerfile 的行号手术，去掉 native client 里依赖被剪掉的测试目标
    sed -i "172d" src/CMakeLists.txt
    sed -i "155d" src/CMakeLists.txt
    mvn clean package -DskipTests -Pdist,native -Dtar
    cp -rf src/main/native/libhdfs/include/* /usr/local/include/
    cp -rf target/target/usr/local/lib/* /usr/local/lib/
    cp -rf target/hadoop-hdfs-native-client-3.2.0/include/* /usr/local/include/
    ldconfig
    rm -rf /opt/buildtools/hadoop /opt/buildtools/boostkit-bigdata
}

install_fmt_10_1_1() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 --branch 10.1.1 https://gitee.com/mirrors/fmt.git
    cd fmt && mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DFMT_TEST=OFF -DFMT_DOC=OFF -DFMT_INSTALL=ON -DBUILD_SHARED_LIBS=ON
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/fmt
}

# folly 静态 + PIC，前置依赖由 operator-pkgs（double-conversion/libevent/glog）和 snappy/lz4/zstd 提供
install_folly_2024_07_01_00() {
    mkdir -p /opt/buildtools && cd /opt/buildtools
    git clone --depth 1 https://gitee.com/mirrors/folly.git
    cd folly
    git fetch --depth 1 origin tag v2024.07.01.00
    git checkout v2024.07.01.00
    mkdir -p build && cd build
    cmake .. -DFOLLY_HAVE_INT128_T=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    make -j"$BUILD_PARALLELISM" && make install
    rm -rf /opt/buildtools/folly
}

main() {
    if [[ $# -eq 1 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
        usage
        return 0
    fi
    if [[ $# -eq 1 ]]; then
        case "$1" in
            --install-base-pkgs)
                install_base_pkgs
                return 0
                ;;
            --install-all-tools)
                install_all_tools
                return 0
                ;;
            --install-all-libs)
                install_all_libs
                return 0
                ;;
        esac
    fi
    if [[ $# -ne 2 ]]; then
        echo "Expected --install <component> / --install-all-tools / --install-all-libs /  --setup <type>." >&2
        usage >&2
        return 1
    fi
    local opt="$1"
    local target="$2"
    case "${opt}" in
        --install)
            if ! is_supported "$target" "${supported_install[@]}"; then
                echo "Unsupported install target: $target" >&2
                usage >&2
                return 1
            fi
            "install_${target//[.-]/_}"
            ;;
        --setup)
            if ! is_supported "$target" "${supported_setup[@]}"; then
                echo "Unsupported setup type: $target" >&2
                usage >&2
                return 1
            fi
            "setup_${target//[.-]/_}"
            ;;
        *)
            echo "Unknown option: $opt" >&2
            usage >&2
            return 1
            ;;
    esac
}

main "$@"
