# OmniOperator Source Build Guide<a name="source_build_guide"></a>

This document describes the source build procedures for OmniOperator and the Gluten Omni backend, including environment preparation, dependency installation, build artifact verification, and release packaging.

## Build Overview<a name="build_overview"></a>

This document describes the complete procedure for building OmniOperator from source on the internet_commercial branch in the openEuler 24.03 LTS aarch64 environment. The procedure is organized in the sequence of environment preparation, source package preparation, dependency installation, building, build result verification, and release packaging. Before starting, check the environment against [**Table 1** Environment requirements](#build_environment_requirements), and then perform the operations in the following sections in order.

All commands in this document are executed by the root user, and the working directory is `/opt/omni`.

After OmniOperator is built, if you need to use the Gluten Omni backend, proceed to [Build the Gluten Omni Backend](#build_gluten).

**Table 1** Environment requirements<a id="build_environment_requirements"></a>

|Item|Requirement|
|--|--|
|OS|openEuler 24.03 LTS (the base release and subsequent service packs such as SP1 and SP2 are all supported; no specific SP is required)|
|Processor architecture|aarch64|
|User permission|root|
|Hardware resources|32 CPU cores or more, 64 GB memory or more, and 40 GB available disk space or more are recommended|
|Network|Access to `repo.openeuler.org`, `gitcode.com`, `gitee.com`, `github.com` (configure an available proxy if direct access is unavailable), `mirrors.huaweicloud.com`, and the Maven mirror in use|
|Local files to upload|None. All source code is downloaded or cloned on the server|

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- This guide has been verified on the openEuler 24.03 LTS base release. The build steps do not depend on a specific service pack. The `openEuler-24.03-LTS-SP1-release` branch cloned in [Clone Patch Repositories](#clone_patch_repos) is only used to obtain a fixed source snapshot and is unrelated to the SP version of the OS.
>- The source packages below are downloaded directly from GitHub by default. If GitHub is not directly accessible, replace the `GH` variable in [Download Source Packages](#download_source_packages) with the prefix of your proxy (in the form `https://<proxy-domain>/https://github.com`).

## System Preparation<a name="system_preparation"></a>

### Install the Compilation Toolchain<a name="install_toolchain"></a>

1. Install the basic compilation toolchain.

    ```shell
    dnf install -y git gcc gcc-c++ make cmake ninja-build autoconf automake libtool m4 \
      perl python3 curl wget tar gzip bzip2 xz unzip zip file which findutils procps-ng \
      util-linux patch openssl openssl-devel zlib-devel libffi-devel ncurses-devel krb5-devel \
      java-1.8.0-openjdk-devel
    ```

2. Install the development headers of the compression libraries required by Arrow.

    ```shell
    dnf install -y utf8proc-devel libcurl-devel boost-devel bison flex \
      brotli-devel bzip2-devel lz4-devel snappy-devel libzstd-devel
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >Arrow depends on the development headers of the preceding compression libraries during compilation. If any of them is missing, the CMake phase fails with a dependency-not-found error.

3. Install cyrus-sasl-devel.

    ```shell
    dnf install -y cyrus-sasl-devel
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >Hadoop libhdfs++ needs to locate the SASL library during compilation. If cyrus-sasl-devel is missing, the following error is reported: "Could not find a SASL library (GSASL (gsasl) or Cyrus SASL (libsasl2)".

4. Verify that the compilation toolchain is installed.

    ```shell
    java -version          # The version is 1.8.0_462.
    mvn -v 2>/dev/null     # Maven is not installed yet. See the Install Maven section.
    ninja --version
    ```

### Create Directories<a name="create_directories"></a>

1. Create the directories used during compilation.

    ```shell
    mkdir -p /opt/omni/{src,downloads,logs,tools,adaptor}
    mkdir -p /opt/omni/downloads/arrow-thirdparty
    ```

2. Create the `/opt/Adaptor` symbolic link, which is referenced during Hadoop installation and compilation.

    ```shell
    ln -sfn /opt/omni/adaptor /opt/Adaptor
    ```

### Configure the Dynamic Library Search Path<a name="configure_ld_so"></a>

The default `/etc/ld.so.conf` of openEuler does not contain `/usr/local/lib` or `/usr/local/lib64`, while the dependencies installed in this document are located in these two directories. Therefore, add them to the dynamic library search path manually.

```shell
cat > /etc/ld.so.conf.d/usr-local.conf <<'EOF'
/usr/local/lib
/usr/local/lib64
EOF
ldconfig
```

### Install Maven<a name="install_maven"></a>

1. Download and decompress Maven 3.8.3.

    ```shell
    curl -sSL -o /opt/omni/downloads/apache-maven-3.8.3-bin.tar.gz \
      https://mirrors.huaweicloud.com/apache/maven/maven-3/3.8.3/binaries/apache-maven-3.8.3-bin.tar.gz
    tar -xzf /opt/omni/downloads/apache-maven-3.8.3-bin.tar.gz -C /opt/omni/tools
    ```

2. Configure the Huawei Cloud central repository mirror in `/root/.m2/settings.xml`.

    ```shell
    mkdir -p /root/.m2
    cat > /root/.m2/settings.xml <<'EOF'
    <settings>
      <mirrors>
        <mirror>
          <id>huaweicloud</id>
          <mirrorOf>central</mirrorOf>
          <url>https://mirrors.huaweicloud.com/repository/maven/</url>
        </mirror>
      </mirrors>
    </settings>
    EOF
    ```

### Generate the Environment Variable Script<a name="configure_env_script"></a>

1. Create the environment variable script `/opt/omni/env.sh`.

    ```shell
    cat > /opt/omni/env.sh <<'EOF'
    #!/bin/bash
    export WORK=/opt/omni
    export SRC=/opt/omni/src
    export LOGS=/opt/omni/logs
    export OMNI_SRC=/opt/omni/src/OmniOperator
    export OMNI_HOME=/opt/omni/install
    export DEPS=/usr/local
    export JAVA_HOME=/usr/lib/jvm/java-1.8.0-openjdk
    export MAVEN_HOME=/opt/omni/tools/apache-maven-3.8.3
    export LLVM_HOME=/usr/lib/llvm-15
    export PROTOBUF_HOME=/opt/Gluten
    export OMNI_COMPILER_THREAD_COUNT=64
    export PATH="$PROTOBUF_HOME/bin:$MAVEN_HOME/bin:$LLVM_HOME/bin:/usr/local/bin:$JAVA_HOME/bin:$PATH"
    export CMAKE_PREFIX_PATH="/usr/local:/opt/Gluten:${CMAKE_PREFIX_PATH:-}"
    export C_INCLUDE_PATH="$DEPS/include:$DEPS/include/libboundscheck/include:$LLVM_HOME/include"
    export CPLUS_INCLUDE_PATH="$C_INCLUDE_PATH"
    export LIBRARY_PATH="$OMNI_HOME/lib:$DEPS/lib:$DEPS/lib64:$LLVM_HOME/lib:$JAVA_HOME/jre/lib/aarch64/server"
    export LD_LIBRARY_PATH="$OMNI_HOME/lib:$DEPS/lib:$DEPS/lib64:$LLVM_HOME/lib:$JAVA_HOME/jre/lib/aarch64/server"
    log() { echo "[$(date '+%F %T')] $*"; }
    EOF
    ```

2. Grant the execute permission on the script and make it take effect.

    ```shell
    chmod +x /opt/omni/env.sh
    source /opt/omni/env.sh
    ```

>![](public_sys-resources/icon-notice.gif) **NOTICE:**
>`JAVA_HOME` must use the alternatives symbolic link `/usr/lib/jvm/java-1.8.0-openjdk`, instead of the actual directory with the build number.

### Clone Patch Repositories<a name="clone_patch_repos"></a>

1. Clone the patch repositories and the snappy source repository.

    ```shell
    cd /opt/omni/src
    git clone --depth 1 --branch main https://gitee.com/kunpengcompute/boostkit-bigdata.git
    git clone --depth 1 --branch openEuler-24.03-LTS-SP1-release https://gitee.com/src-openeuler/snappy.git snappy-src
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >`boostkit-bigdata` provides the patches for Arrow and Hadoop, and `snappy-src` provides the snappy source code and two patches.

2. Handle offline environments. Run the preceding clone commands on a machine with network access, package the repositories, copy them to the `/opt/omni/src` directory, and decompress them.

## Prepare Source Packages<a name="prepare_source_packages"></a>

### Download Source Packages<a name="download_source_packages"></a>

1. Download all source packages.

    ```shell
    DL=/opt/omni/downloads
    TP=$DL/arrow-thirdparty
    GH="https://github.com"
    cd "$DL"

    get() { curl -fL --retry 3 --connect-timeout 20 -o "$2" "$1" || echo "FAILED: $2"; }

    get "$GH/apache/arrow/archive/refs/tags/apache-arrow-11.0.0.tar.gz"                arrow-apache-arrow-11.0.0.tar.gz
    get "$GH/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz"                           json-v3.11.3.tar.gz
    get "$GH/jemalloc/jemalloc/archive/refs/tags/5.3.0.tar.gz"                         jemalloc-5.3.0.tar.gz
    get "$GH/madler/zlib/archive/refs/tags/v1.3.1.tar.gz"                              zlib-v1.3.1.tar.gz
    get "$GH/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz"                                 lz4-v1.10.0.tar.gz
    get "$GH/facebook/zstd/archive/refs/tags/v1.5.6.tar.gz"                            zstd-v1.5.6.tar.gz
    get "$GH/cyrusimap/cyrus-sasl/archive/refs/tags/cyrus-sasl-2.1.28.tar.gz"         cyrus-sasl-2.1.28.tar.gz
    get "$GH/zlib-ng/zlib-ng/archive/refs/tags/2.0.4.tar.gz"                           zlib-ng-2.0.4.tar.gz
    get "$GH/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.tar.gz"               jsoncpp-1.9.6.tar.gz
    get "$GH/apache/hadoop/archive/refs/tags/rel/release-3.2.0.tar.gz"                 hadoop-rel-release-3.2.0.tar.gz
    get "$GH/google/googletest/archive/refs/tags/v1.14.0.tar.gz"                       googletest-v1.14.0.tar.gz
    get "$GH/abseil/abseil-cpp/archive/refs/tags/20240722.0.tar.gz"                    abseil-cpp-20240722.0.tar.gz
    get "$GH/google/re2/archive/refs/tags/2024-07-02.tar.gz"                           re2-2024-07-02.tar.gz
    get "$GH/protocolbuffers/protobuf/archive/refs/tags/v3.21.9.tar.gz"                protobuf-v3.21.9.tar.gz
    get "$GH/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz"                        rapidjson-v1.1.0.tar.gz
    get "$GH/facebook/folly/archive/refs/tags/v2024.01.01.00.tar.gz"                   folly-v2024.01.01.00.tar.gz
    get "$GH/llvm/llvm-project/releases/download/llvmorg-15.0.4/llvm-project-15.0.4.src.tar.xz" \
        llvm-project-15.0.4.src.tar.xz

    # Third-party packages used during Arrow build
    get "$GH/microsoft/mimalloc/archive/refs/tags/v2.0.6.tar.gz"                       "$TP/mimalloc-v2.0.6.tar.gz"
    get "$GH/substrait-io/substrait/archive/refs/tags/v0.20.0.tar.gz"                  "$TP/substrait-v0.20.0.tar.gz"
    get "$GH/google/re2/archive/refs/tags/2022-06-01.tar.gz"                           "$TP/re2-2022-06-01.tar.gz"
    get "$GH/apache/thrift/archive/refs/tags/v0.16.0.tar.gz"                           "$TP/thrift-0.16.0.tar.gz"
    get "$GH/xtensor-stack/xsimd/archive/refs/tags/9.0.1.tar.gz"                       "$TP/xsimd-9.0.1.tar.gz"
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The thrift tag has a `v` prefix. If it is written as `0.16.0`, the request returns 404. The top-level directory of the archive is still `thrift-0.16.0`.

2. Check the download result to confirm the file size and integrity. Files that fail to be downloaded are marked with "FAILED".

    ```shell
    ls -l $DL/*.tar.* $DL/*.xz $TP/*.tar.gz
    ```

### Repack Protobuf v21.3<a name="repack_protobuf"></a>

The ExternalProject of Arrow requires the top-level directory of the archive to be `protobuf-21.3`. Therefore, clone the v21.3 branch from GitHub and repack it.

```shell
cd /opt/omni/src
rm -rf protobuf-21.3
git clone --depth 1 -b v21.3 https://github.com/protocolbuffers/protobuf.git protobuf-21.3

tar -czf /opt/omni/downloads/arrow-thirdparty/protobuf-v21.3.tar.gz protobuf-21.3
sha256sum /opt/omni/downloads/arrow-thirdparty/protobuf-v21.3.tar.gz
```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>The clone directory is named `protobuf-21.3`, so the top-level directory of the repacked archive meets the Arrow naming requirement. The [Install Arrow](#install_arrow) section recalculates the checksum and writes it to `versions.txt` accordingly.
>![](public_sys-resources/icon-notice.gif) **NOTICE:**
>The Protobuf v21.3 here is used only for the internal build of Arrow. Do not confuse it with the Protobuf 3.21.9 installed on the system (see [Install Protobuf 3.21.9](#install_protobuf)).

### Repack jemalloc 5.3.0<a name="repack_jemalloc"></a>

The GitHub archive does not contain the `configure` script, and the ExternalProject of Arrow does not run `autogen.sh` automatically. Therefore, generate `configure` first and then repack the source code.

```shell
cd /opt/omni/src
rm -rf jemalloc-tp && mkdir jemalloc-tp
tar -xzf /opt/omni/downloads/jemalloc-5.3.0.tar.gz -C jemalloc-tp --strip-components=1

cd jemalloc-tp
echo "5.3.0" > VERSION
./autogen.sh --disable-initial-exec-tls --with-lg-page=16

cd /opt/omni/src
tar -czf /opt/omni/downloads/arrow-thirdparty/jemalloc-5.3.0.tar.gz \
  --transform 's,^jemalloc-tp,jemalloc-5.3.0,' jemalloc-tp
sha256sum /opt/omni/downloads/arrow-thirdparty/jemalloc-5.3.0.tar.gz
```

### Obtain ORC Source Code<a name="obtain_orc"></a>

The ORC source code is hosted in the public [repository](https://gitcode.com/boostkit/orc). Clone the `v1.7.4_new` branch directly.

```shell
cd /opt/omni/src
rm -rf orc
git clone -b v1.7.4_new https://gitcode.com/boostkit/orc.git orc
```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>The top-level directory of the cloned `orc` repository is the root directory of the ORC source code (containing `CMakeLists.txt`, `c++`, `proto`, `tools`, and so on). The [Install ORC 1.7.4](#install_orc) section builds ORC in the `/opt/omni/src/orc` directory.

## Install Common Dependencies<a name="install_common_dependencies"></a>

This section describes how to install the common dependencies required for building OmniOperator. Perform the operations in the following sections in order.

### Install Protobuf 3.21.9<a name="install_protobuf"></a>

Hadoop libhdfspp, ORC, and the protoc plug-in of Maven all depend on Protobuf. In this document, Protobuf is installed in the `/opt/Gluten` directory.

1. Build and install Protobuf.

    ```shell
    cd /opt/omni/src
    rm -rf protobuf && mkdir protobuf
    tar -xzf /opt/omni/downloads/protobuf-v3.21.9.tar.gz -C protobuf --strip-components=1

    cd protobuf
    ./autogen.sh
    ./configure --enable-static=no --prefix=/opt/Gluten
    make -j32
    make install
    ```

2. Verify that Protobuf is installed.

    ```shell
    /opt/Gluten/bin/protoc --version    # The version is libprotoc 3.21.9.
    ```

### Install Arrow 11.0.0<a name="install_arrow"></a>

1. Decompress the source package and apply the patch.

    ```shell
    cd /opt/omni/src
    rm -rf arrow && mkdir arrow
    tar -xzf /opt/omni/downloads/arrow-apache-arrow-11.0.0.tar.gz -C arrow --strip-components=1

    cp boostkit-bigdata/omnioperator/contrib/arrow-maint-11_0_0/arrow-maint-11_0_0.patch arrow/
    cd arrow
    patch -p1 < arrow-maint-11_0_0.patch
    ```

2. Point the third-party packages to local files and write back the checksums.

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The third-party packages such as mimalloc, protobuf, and jemalloc are repacked locally, so their SHA256 values are different from the official checksums. Recalculate the values and write them to `cpp/thirdparty/versions.txt`; otherwise, the build fails directly.

    ```shell
    TP=/opt/omni/downloads/arrow-thirdparty

    fix_sha() {
      local key=$1 file=$2
      local sha
      sha=$(sha256sum "$file" | awk '{print $1}')
      sed -i "s|^${key}=.*|${key}=${sha}|" cpp/thirdparty/versions.txt
      echo "$key=$sha"
    }

    fix_sha ARROW_MIMALLOC_BUILD_SHA256_CHECKSUM  "$TP/mimalloc-v2.0.6.tar.gz"
    fix_sha ARROW_PROTOBUF_BUILD_SHA256_CHECKSUM  "$TP/protobuf-v21.3.tar.gz"
    fix_sha ARROW_SUBSTRAIT_BUILD_SHA256_CHECKSUM "$TP/substrait-v0.20.0.tar.gz"
    fix_sha ARROW_JEMALLOC_BUILD_SHA256_CHECKSUM  "$TP/jemalloc-5.3.0.tar.gz"
    fix_sha ARROW_RE2_BUILD_SHA256_CHECKSUM       "$TP/re2-2022-06-01.tar.gz"
    fix_sha ARROW_THRIFT_BUILD_SHA256_CHECKSUM    "$TP/thrift-0.16.0.tar.gz"
    fix_sha ARROW_XSIMD_BUILD_SHA256_CHECKSUM     "$TP/xsimd-9.0.1.tar.gz"
    ```

3. Build and install Arrow.

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >`ARROW_DATASET` must be set to `ON` because the `reader` module depends on `ArrowDataset`.

    ```shell
    export ARROW_MIMALLOC_URL=file://$TP/mimalloc-v2.0.6.tar.gz
    export ARROW_PROTOBUF_URL=file://$TP/protobuf-v21.3.tar.gz
    export ARROW_SUBSTRAIT_URL=file://$TP/substrait-v0.20.0.tar.gz
    export ARROW_JEMALLOC_URL=file://$TP/jemalloc-5.3.0.tar.gz
    export ARROW_RE2_URL=file://$TP/re2-2022-06-01.tar.gz
    export ARROW_THRIFT_URL=file://$TP/thrift-0.16.0.tar.gz
    export ARROW_XSIMD_URL=file://$TP/xsimd-9.0.1.tar.gz

    mkdir -p cpp/_build && cd cpp/_build
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DARROW_JEMALLOC_LG_PAGE=16 \
      -DARROW_BUILD_STATIC=OFF \
      -DARROW_BUILD_TESTS=OFF \
      -DARROW_ENABLE_TIMING_TESTS=OFF \
      -DARROW_BUILD_INTEGRATION=OFF \
      -DARROW_COMPUTE=ON \
      -DARROW_DATASET=ON \
      -DARROW_EXTRA_ERROR_CONTEXT=ON \
      -DARROW_MIMALLOC=ON \
      -DARROW_SUBSTRAIT=ON \
      -DARROW_WITH_BROTLI=ON -DARROW_WITH_BZ2=ON -DARROW_WITH_LZ4=ON \
      -DARROW_WITH_SNAPPY=ON -DARROW_WITH_UTF8PROC=ON -DARROW_WITH_ZLIB=ON \
      -DARROW_WITH_ZSTD=ON -DARROW_HDFS=ON
    make -j32
    make install
    ```

4. Verify that Arrow is installed.

    ```shell
    grep ARROW_VERSION_STRING /usr/local/include/arrow/util/config.h   # "11.0.0"
    ls /usr/local/lib64/cmake/ArrowDataset
    ```

### Install nlohmann/json 3.11.3<a name="install_json"></a>

```shell
cd /opt/omni/src
rm -rf json && mkdir json
tar -xzf /opt/omni/downloads/json-v3.11.3.tar.gz -C json --strip-components=1
cmake -S json -B json/build -DCMAKE_BUILD_TYPE=Release -DJSON_BuildTests=OFF -DJSON_Install=ON
cmake --build json/build --parallel 32
cmake --install json/build
```

### Install jemalloc 5.3.0<a name="install_jemalloc"></a>

```shell
cd /opt/omni/src
rm -rf jemalloc && mkdir jemalloc
tar -xzf /opt/omni/downloads/arrow-thirdparty/jemalloc-5.3.0.tar.gz -C jemalloc --strip-components=1
cd jemalloc
./autogen.sh --disable-initial-exec-tls --with-lg-page=16
make -j32
make install
```

Verify that jemalloc is installed. Run `ldconfig -p | grep jemalloc` to query the library. The header file is located in `/usr/local/include/jemalloc/jemalloc.h`.

### Install zlib 1.3.1<a name="install_zlib"></a>

```shell
cd /opt/omni/src
rm -rf zlib && mkdir zlib
tar -xzf /opt/omni/downloads/zlib-v1.3.1.tar.gz -C zlib --strip-components=1
cd zlib
./configure --prefix=/usr/local
make -j32
make install
```

### Install LZ4 1.10.0<a name="install_lz4"></a>

```shell
cd /opt/omni/src
rm -rf lz4 && mkdir lz4
tar -xzf /opt/omni/downloads/lz4-v1.10.0.tar.gz -C lz4 --strip-components=1
cd lz4
make -j32
make install PREFIX=/usr/local
```

### Install Snappy 1.1.10<a name="install_snappy"></a>

```shell
cd /opt/omni/src
rm -rf snappy && mkdir snappy
tar -xzf snappy-src/snappy-1.1.10.tar.gz -C snappy --strip-components=1

cd snappy
patch -p1 < ../snappy-src/add-option-to-enable-rtti-set-default-to-current-ben.patch
patch -p1 < ../snappy-src/remove-dependency-on-google-benchmark-and-gmock.patch

rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DSNAPPY_BUILD_BENCHMARKS=OFF -DSNAPPY_BUILD_TESTS=OFF \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j32
make install
```

### Install Zstandard 1.5.6<a name="install_zstd"></a>

```shell
cd /opt/omni/src
rm -rf zstd && mkdir zstd
tar -xzf /opt/omni/downloads/zstd-v1.5.6.tar.gz -C zstd --strip-components=1
cd zstd
make -j32 PREFIX=/usr/local
make install PREFIX=/usr/local
```

### Install Cyrus SASL 2.1.28<a name="install_cyrus_sasl"></a>

```shell
cd /opt/omni/src
rm -rf cyrus-sasl && mkdir cyrus-sasl
tar -xzf /opt/omni/downloads/cyrus-sasl-2.1.28.tar.gz -C cyrus-sasl --strip-components=1
cd cyrus-sasl
./autogen.sh
make -j32
make install
```

### Install zlib-ng 2.0.4<a name="install_zlib_ng"></a>

```shell
cd /opt/omni/src
rm -rf zlib-ng && mkdir zlib-ng
tar -xzf /opt/omni/downloads/zlib-ng-2.0.4.tar.gz -C zlib-ng --strip-components=1
cd zlib-ng
./configure --prefix=/usr/local
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j32
make install
```

### Install jsoncpp 1.9.6<a name="install_jsoncpp"></a>

```shell
cd /opt/omni/src
rm -rf jsoncpp && mkdir jsoncpp
tar -xzf /opt/omni/downloads/jsoncpp-1.9.6.tar.gz -C jsoncpp --strip-components=1
rm -rf jsoncpp/build && mkdir -p jsoncpp/build && cd jsoncpp/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF
make -j32
make install
```

### Install Hadoop 3.2.0<a name="install_hadoop"></a>

1. Decompress the source package and apply the patch.

    ```shell
    source /opt/omni/env.sh

    cd /opt/omni/src
    rm -rf hadoop && mkdir hadoop
    tar -xzf /opt/omni/downloads/hadoop-rel-release-3.2.0.tar.gz -C hadoop --strip-components=1

    cp boostkit-bigdata/omnioperator/contrib/hadoop-3_2_0/hadoop-3_2_0.patch hadoop/
    cd hadoop
    patch -p1 < hadoop-3_2_0.patch
    ```

2. Build the native part of Hadoop.

    ```shell
    cd /opt/omni/src/hadoop/hadoop-hdfs-project/hadoop-hdfs-native-client
    mvn clean package -DskipTests -Pdist,native -Dtar
    ```

3. Install the header files and libraries to `/opt/Adaptor`.

    ```shell
    cd /opt/omni/src/hadoop/hadoop-hdfs-project/hadoop-hdfs-native-client
    mkdir -p /opt/Adaptor/include
    cp -rf src/main/native/libhdfs/include/* /opt/Adaptor/include/
    cp target/hadoop-hdfs-native-client-3.2.0/include/hdfs.h /opt/Adaptor/include/   # If the file does not exist, use target/target/usr/local/include/hdfs.h.
    cp -rf src/main/native/libhdfspp/include /opt/Adaptor/
    cp -rf target/target/usr/local/lib /opt/Adaptor/
    cp -f target/main/native/libhdfspp/libhdfspp.so* /opt/Adaptor/lib/
    ```

4. Verify that Hadoop is installed.

    ```shell
    ls /opt/Adaptor/lib       # libhdfs.so.0.0.0, libhdfspp.so.0.1.0
    ls /opt/Adaptor/include   # hdfs  hdfs.h  hdfspp
    ```

### Install GoogleTest 1.14.0<a name="install_googletest"></a>

```shell
cd /opt/omni/src
rm -rf googletest && mkdir googletest
tar -xzf /opt/omni/downloads/googletest-v1.14.0.tar.gz -C googletest --strip-components=1
cmake -S googletest -B googletest/build -DCMAKE_BUILD_TYPE=Release
cmake --build googletest/build --parallel 32
cmake --install googletest/build
```

### Install Abseil-cpp 20240722.0<a name="install_abseil"></a>

```shell
cd /opt/omni/src
rm -rf abseil-cpp && mkdir abseil-cpp
tar -xzf /opt/omni/downloads/abseil-cpp-20240722.0.tar.gz -C abseil-cpp --strip-components=1
cmake -S abseil-cpp -B abseil-cpp/build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build abseil-cpp/build --parallel 32
cmake --install abseil-cpp/build
```

### Install RE2 2024-07-02<a name="install_re2"></a>

```shell
cd /opt/omni/src
rm -rf re2 && mkdir re2
tar -xzf /opt/omni/downloads/re2-2024-07-02.tar.gz -C re2 --strip-components=1
cmake -S re2 -B re2/build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_PREFIX_PATH=/usr/local
cmake --build re2/build --parallel 32
cmake --install re2/build
```

### Install libboundscheck<a name="install_libboundscheck"></a>

`vector`, `util`, and `codegen` are all linked with `-lboundscheck`, and the include path of the source code is `<libboundscheck/include/securec.h>`. Therefore, the header file directory hierarchy must be retained during installation.

1. Build and install libboundscheck.

    ```shell
    cd /opt/omni/src
    rm -rf libboundscheck
    git clone https://gitcode.com/openeuler/libboundscheck.git libboundscheck

    cd libboundscheck
    git checkout 6ad12812c133297b1580d7da6e197af3bae9c749
    make CC=gcc -j32

    mkdir -p /usr/local/include/libboundscheck/include /usr/local/lib
    cp -a include/. /usr/local/include/libboundscheck/include/
    cp -f lib/libboundscheck.so /usr/local/lib/
    ldconfig
    ```

2. Verify that libboundscheck is installed.

    ```shell
    ls /usr/local/lib/libboundscheck.so
    ls /usr/local/include/libboundscheck/include/securec.h
    ```

### Install RapidJSON 1.1.0<a name="install_rapidjson"></a>

RapidJSON is a header-only library. You only need to copy the header files.

```shell
cd /opt/omni/src
tar -zxf /opt/omni/downloads/rapidjson-v1.1.0.tar.gz -C /opt/omni/src
mkdir -p /usr/local/include
cp -a /opt/omni/src/rapidjson-1.1.0/include/rapidjson /usr/local/include/
```

Verify that RapidJSON is installed. If the `/usr/local/include/rapidjson/document.h` file exists, the installation is successful.

### Install LLVM/Clang 15.0.4<a name="install_llvm"></a>

The `codegen` module hardcodes the `/usr/lib/llvm-15/include` directory and links `-lLLVM-15`. Therefore, LLVM must be installed in this path, and the dynamic library must be generated.

1. Build and install LLVM/Clang.

    ```shell
    cd /opt/omni/src
    tar -xf /opt/omni/downloads/llvm-project-15.0.4.src.tar.xz
    # The top-level directory is llvm-project-15.0.4.src.

    rm -rf /opt/omni/src/llvm-build-15.0.4
    cmake -G Ninja -S /opt/omni/src/llvm-project-15.0.4.src/llvm \
      -B /opt/omni/src/llvm-build-15.0.4 \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_TARGETS_TO_BUILD="AArch64" \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_BUILD_LLVM_DYLIB=ON \
      -DLLVM_LINK_LLVM_DYLIB=ON \
      -DLLVM_INSTALL_UTILS=ON \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr/lib/llvm-15

    cmake --build /opt/omni/src/llvm-build-15.0.4 --parallel 64
    cmake --install /opt/omni/src/llvm-build-15.0.4
    ```

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >Both `LLVM_ENABLE_RTTI=ON` and `LLVM_BUILD_LLVM_DYLIB=ON` are mandatory. Otherwise, the compilation or linking of codegen fails.

2. Configure the dynamic library path and the symbolic links available to clang++.

    ```shell
    ln -sf /usr/lib/llvm-15/lib/libLLVM-15.so /usr/local/lib64/libLLVM-15.so

    echo /usr/lib/llvm-15/lib > /etc/ld.so.conf.d/llvm-15.conf
    ldconfig

    for b in clang clang++ llvm-config llvm-ar llvm-link opt llc llvm-as; do
      ln -sf /usr/lib/llvm-15/bin/$b /usr/local/bin/$b
    done

    # The GCC triple of openEuler is aarch64-openEuler-linux, while clang 15 looks for aarch64-unknown-linux-gnu by default.
    ln -sfn /usr/lib/gcc/aarch64-openEuler-linux /usr/lib/gcc/aarch64-unknown-linux-gnu
    ln -sfn /usr/include/c++/12/aarch64-openEuler-linux /usr/include/c++/12/aarch64-unknown-linux-gnu
    ```

3. Verify that LLVM/Clang is installed.

    ```shell
    llvm-config --version        # 15.0.4
    clang --version | head -1
    ldconfig -p | grep libLLVM-15
    ```

## Install Additional Dependencies for the internet_commercial Branch<a name="install_extra_dependencies"></a>

The `vectorization` and `reader` modules introduce the following dependencies, which are not included in the common dependencies and need to be installed separately.

### Install System Packages<a name="install_extra_system_packages"></a>

```shell
dnf install -y fmt-devel double-conversion-devel gflags-devel glog-devel \
  libevent-devel libunwind-devel libsodium-devel boost-devel lld
```

>![](public_sys-resources/icon-notice.gif) **NOTICE:**
>`lld` is mandatory for `build.sh`, which uses `-fuse-ld=lld` for linking.

### Install folly<a name="install_folly"></a>

The repository does not provide the `folly-devel` package, so folly must be built from source.

1. Build folly.

    ```shell
    cd /opt/omni/src
    rm -rf folly && mkdir folly
    tar -xzf /opt/omni/downloads/folly-v2024.01.01.00.tar.gz -C folly --strip-components=1

    # The build directory cannot be named build:
    # build/fbcode_builder/CMake/FBBuildOptions.cmake in the folly source code is a build dependency.
    cd folly && rm -rf _build && mkdir _build && cd _build

    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib64/cmake;/usr/lib64" \
      -DCMAKE_CXX_STANDARD=17

    make -j32
    ```

2. Install folly.

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The sample program `logging_example` may fail to compile due to a snappy symbolic link issue. If `libfolly.so` has been generated, skip the sample and install folly directly.

    ```shell
    cmake --install .
    ldconfig
    ```

3. Verify that folly is installed. If the `/usr/local/lib/libfolly.so` file and the `/usr/local/lib/cmake/folly` directory exist, the installation is successful.

### Install ORC 1.7.4<a name="install_orc"></a>

The `reader` module strongly depends on ORC, so ORC must be installed first. The ORC source code has been cloned to `/opt/omni/src/orc` in the [Obtain ORC Source Code](#obtain_orc) section, and its top-level directory is the root directory of the source code.

1. Confirm the ORC source code directory.

    ```shell
    ls /opt/omni/src/orc        # CMakeLists.txt  c++  cmake_modules  proto  tools ...
    ```

2. Set the environment variables required for building ORC.

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >All `*_HOME` environment variables must be set. Otherwise, ORC downloads and builds lz4, zstd, zlib, and snappy again through ExternalProject, and the snappy build fails.

    ```shell
    export PROTOBUF_HOME=/opt/Gluten
    export ZSTD_HOME=/usr/local
    export LZ4_HOME=/usr/local
    export ZLIB_HOME=/usr/local
    export ZLIB_NG_HOME=/usr/local
    export SNAPPY_HOME=/usr/local
    export PATH=/opt/Gluten/bin:$PATH
    ```

3. Build and install ORC.

    ```shell
    cd /opt/omni/src/orc
    rm -rf build && mkdir build && cd build

    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_PREFIX_PATH="/opt/Gluten;/usr/local" \
      -DBUILD_JAVA=OFF \
      -DANALYZE_JAVA=OFF \
      -DBUILD_LIBHDFSPP=OFF \
      -DBUILD_CPP_TESTS=OFF \
      -DBUILD_TOOLS=ON \
      -DBUILD_POSITION_INDEPENDENT_LIB=ON

    make -j32
    make install
    ldconfig
    ```

### Generate liborc.so<a name="generate_liborc"></a>

In the `CMakeLists.txt` of ORC 1.7.4, the library is fixed as `add_library(orc STATIC ...)`, so only the static library `liborc.a` is generated. `reader.so` requires a shared library when linking with `-lorc`; otherwise, dependencies such as protobuf, zlib, lz4, zstd, and snappy cannot be passed.

1. Generate the shared library `liborc.so` from the static library.

    ```shell
    ORC_A=/opt/omni/src/orc/build/c++/src/liborc.a

    g++ -shared -o /usr/local/lib/liborc.so.1.7.4 \
      -Wl,--whole-archive "$ORC_A" -Wl,--no-whole-archive \
      -lprotobuf -lz -llz4 -lzstd -lsnappy -lpthread \
      -L/opt/Gluten/lib -L/usr/local/lib64 -L/usr/local/lib

    ln -sf liborc.so.1.7.4 /usr/local/lib/liborc.so

    echo "/opt/Gluten/lib" > /etc/ld.so.conf.d/gluten.conf
    ldconfig
    ```

2. Verify that all dependencies of `liborc.so` are resolved.

    ```shell
    ldd /usr/local/lib/liborc.so.1.7.4 | grep 'not found'   # No output indicates that the dependencies are resolved.
    ```

## Build OmniOperator<a name="build_omnioperator"></a>

### Obtain the Source Code and Switch Branches<a name="obtain_omnioperator_source"></a>

1. Clone the OmniOperator source code and switch to the internet_commercial branch.

    ```shell
    cd /opt/omni/src
    rm -rf OmniOperator
    git clone https://gitcode.com/openeuler/OmniOperator.git
    cd OmniOperator

    git fetch https://gitcode.com/openeuler/OmniOperator.git \
      internet_commercial:refs/remotes/openeuler/internet_commercial
    git checkout -B internet_commercial refs/remotes/openeuler/internet_commercial
    ```

2. Create the `clang++-15` symbolic link.

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The top-level `CMakeLists.txt` uses `clang++-15` for compilation. openEuler does not provide this command by default, so create a symbolic link.

    ```shell
    ln -sf /usr/lib/llvm-15/bin/clang++ /usr/local/bin/clang++-15
    ```

### Build the Native Libraries<a name="build_native"></a>

1. Set the environment variables required for compilation.

    ```shell
    source /opt/omni/env.sh

    cd /opt/omni/src/OmniOperator
    export OMNI_HOME=/opt/omni/install_internet
    export OMNI_COMPILER_THREAD_COUNT=32

    # The compute module references the Protobuf io header but does not propagate the include path.
    # The ORC header uses relative includes internally, so the orc directory itself must be added to the search path.
    export CXXFLAGS="-I/opt/Gluten/include -I/opt/Adaptor/include \
      -I/usr/local/include -I/usr/local/include/orc"

    # libhdfs exists only in /opt/Adaptor/lib.
    export LIBRARY_PATH="/opt/Adaptor/lib:/opt/Gluten/lib:/usr/local/lib64:/usr/local/lib"
    ```

2. Perform the build.

    ```shell
    bash build_scripts/build.sh release:java --exclude-test
    ```

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >- Setting `OMNI_COMPILER_THREAD_COUNT` to 256 is not recommended. When openEuler GCC 12 compiles SVE code with high concurrency, the error `internal compiler error: Bus error ... during RTL pass: cprop` may be triggered. The build is stable when the value is set to 32.
    >- `build.sh` deletes `build/CMakeCache.txt` each time it is executed, so the preceding environment variables take effect again in every build.
    >- `build.sh release:java` builds only the native part and does not generate JAR packages. For details about how to generate JAR packages, see [Generate JAR Packages](#build_jar).

### Generate JAR Packages<a name="build_jar"></a>

1. Build the Java binding.

    ```shell
    cd /opt/omni/src/OmniOperator/bindings/java
    mvn -B clean install -Domni.home="$OMNI_HOME" -DskipTests
    ```

2. Build the UDF.

    ```shell
    cd /opt/omni/src/OmniOperator/core/src/udf/java
    mvn -B clean install -DskipTests
    ```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>Maven needs to access remote repositories. The Huawei Cloud mirror has been configured in the [Install Maven](#install_maven) section. In a completely offline environment, preheat the `~/.m2/repository` directory or copy the entire directory in advance.

## Verify the Build Results<a name="verify_build"></a>

### Check the Artifacts<a name="check_artifacts"></a>

1. Check the library files and Java binding artifacts.

    ```shell
    ls -lh /opt/omni/install_internet/lib
    ls -lh /opt/omni/install_internet/java-binding
    ```

    The artifact directory structure is as follows.

    ```text
    /opt/omni/install_internet/
    ├── lib/
    │   ├── libboostkit-omniop-vector-2.2.0-aarch64.so
    │   ├── libboostkit-omniop-codegen-2.2.0-aarch64.so
    │   ├── libboostkit-omniop-operator-2.2.0-aarch64.so
    │   ├── libboostkit-omniop-reader-2.2.0-aarch64.so
    │   └── include/
    └── java-binding/
        └── libboostkit-omniop-java-binding-2.2.0-aarch64.so
    ```

2. Check the JAR package artifacts.

    ```text
    bindings/java/target/boostkit-omniop-bindings-2.2.0-aarch64.jar
    core/src/udf/java/target/boostkit-omniop-udf-2.2.0-aarch64.jar
    ```

### Check Dynamic Library Dependencies<a name="check_dynamic_dependencies"></a>

1. Check the dynamic library dependencies of the `reader` module.

    ```shell
    ldd /opt/omni/install_internet/lib/libboostkit-omniop-reader-2.2.0-aarch64.so | grep 'not found'
    ```

    No output indicates that the dependencies are resolved.

2. Confirm that the key dependencies are resolved correctly.

    ```output
    libarrow_dataset.so.1100 -> /usr/local/lib64/libarrow_dataset.so.1100
    libparquet.so.1100       -> /usr/local/lib64/libparquet.so.1100
    liborc.so                -> /usr/local/lib/liborc.so
    libhdfs.so.0.0.0         -> /opt/Adaptor/lib/libhdfs.so.0.0.0
    libjvm.so                -> /usr/lib/jvm/java-1.8.0-openjdk/jre/lib/aarch64/server/libjvm.so
    libprotobuf.so.32        -> /opt/Gluten/lib/libprotobuf.so.32
    ```

### Configure Runtime Library Paths<a name="configure_runtime_path"></a>

1. Write the library directories required at runtime to the dynamic library configuration file.

    ```shell
    cat > /etc/ld.so.conf.d/omni-adaptor.conf <<'EOF'
    /opt/Adaptor/lib
    /usr/lib/jvm/java-1.8.0-openjdk/jre/lib/aarch64/server
    EOF
    ldconfig
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >`/opt/Gluten/lib` has been written to `/etc/ld.so.conf.d/gluten.conf` in the [Generate liborc.so](#generate_liborc) section. `/usr/local/lib`, `/usr/local/lib64`, and `/usr/lib/llvm-15/lib` are configured in the [Configure the Dynamic Library Search Path](#configure_ld_so) and [Install LLVM/Clang 15.0.4](#install_llvm) sections, respectively.

2. Set the runtime environment variables.

    ```shell
    export LD_LIBRARY_PATH=/opt/omni/install_internet/lib:/opt/omni/install_internet/java-binding:$LD_LIBRARY_PATH
    ```

## Build the Gluten Omni Backend<a name="build_gluten"></a>

This chapter describes the complete procedure for building the Omni backend (backends-omni) of the Gluten project internet_commercial branch from source code based on the OmniOperator build artifacts. The build artifacts are the C++ execution plugin `libspark_columnar_plugin.so` and the Spark plugin package `gluten-omni-bundle-*.jar`.

The Gluten C++ plugin directly links the `boostkit-omniop-*` dynamic libraries in `/opt/omni/install_internet/lib` described in the [Verify the Build Results](#verify_build) section, and the Java side depends on the `boostkit-omniop-bindings` installed to the local Maven repository by the same build. Therefore, OmniOperator must be built and deployed first.

All commands in this chapter are executed by the root user by default, and the Gluten source directory is `/opt/omni/src/Gluten`.

>![](public_sys-resources/icon-notice.gif) **NOTICE:**
>This chapter builds only the Omni backend and does not involve the Velox backend. Do not run any script whose name contains "velox", such as `dev/builddeps-veloxbe.sh` or `dev/buildbundle-veloxbe.sh`.

### Install the Gluten Build Dependencies<a name="gluten_install_toolchain"></a>

1. Install sudo.

    ```shell
    dnf install -y sudo
    ```

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >`cpp-omni/build.sh` internally runs `sudo make install`. A bare-metal machine does not provide the `sudo` command by default, so the error `sudo: command not found` is reported when it is missing.

2. Verify the compilation toolchain.

    ```shell
    gcc --version | head -1        # 12.3.1
    cmake --version | head -1      # 3.27.9
    ninja --version                # 1.11.1
    java -version                  # 1.8.0_462
    mvn -v | head -1               # Apache Maven 3.8.3
    ```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- The basic compilation toolchain has been installed in the [Install the Compilation Toolchain](#install_toolchain) section, and dependencies such as fmt and folly have been installed in the [Install Additional Dependencies for the internet_commercial Branch](#install_extra_dependencies) section. No repeated installation is required.
>- The Huawei Cloud central Maven mirror has been configured in the [Install Maven](#install_maven) section and is reused directly when building Gluten.

### Configure Gluten Runtime Library Paths<a name="gluten_configure_ld_so"></a>

The C++ plugin depends on the dynamic libraries of OmniOperator and Protobuf at runtime, so the dynamic library configuration must be written in advance.

```shell
printf '/opt/omni/install_internet/lib\n' > /etc/ld.so.conf.d/gluten-omni.conf
ldconfig
```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>`/opt/Gluten/lib` (Protobuf 3.21.9) has been written to `/etc/ld.so.conf.d/gluten.conf` in the [Generate liborc.so](#generate_liborc) section, and `/usr/local/lib` and `/usr/local/lib64` have been configured in the [Configure the Dynamic Library Search Path](#configure_ld_so) section. No repeated addition is required.

### Obtain the Gluten Source Code<a name="gluten_obtain_source"></a>

1. Clone the internet_commercial branch.

    ```shell
    cd /opt/omni/src
    rm -rf Gluten
    git clone --depth 1 -b internet_commercial https://gitcode.com/openeuler/Gluten.git
    cd Gluten
    git log --oneline -1        # 290c9b6 !363 feat: introduce Celeborn for Shuffle
    ```

2. Confirm that the Omni backend directories exist.

    ```shell
    ls cpp-omni backends-omni
    ```

    `cpp-omni` contains the C++ execution plugin source code and build scripts, and `backends-omni` is the Java/Scala Omni backend module.

### Prepare the Gluten Build Environment Variables<a name="gluten_prepare_env"></a>

Create the environment variable script `/opt/omni/gluten_env.sh`.

```shell
cat > /opt/omni/gluten_env.sh <<'EOF'
#!/bin/bash
source /opt/omni/env.sh

export OMNI_HOME=/opt/omni/install_internet
export GLUTEN_HOME=/opt/omni/src/Gluten
export JAVA_HOME=/usr/lib/jvm/java-1.8.0-openjdk

export CMAKE_PREFIX_PATH="/opt/Gluten:/usr/local:${CMAKE_PREFIX_PATH:-}"
export LIBRARY_PATH="/opt/omni/install_internet/lib:/opt/Gluten/lib:/usr/local/lib:/usr/local/lib64:${LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="/opt/omni/install_internet/lib:/opt/Gluten/lib:/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-}"

# ORC头文件内部使用相对包含（Adaptor.hh），需要把orc目录本身加入搜索路径
export CXXFLAGS="-I/opt/Adaptor/include -I/usr/local/include/orc -I/opt/Gluten/include"
EOF
chmod +x /opt/omni/gluten_env.sh
source /opt/omni/gluten_env.sh
```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- `build.sh` concatenates `$OMNI_HOME/lib/include` as the OmniOperator header file path based on `OMNI_HOME`. Therefore, `OMNI_HOME` must point to the actual installation directory `/opt/omni/install_internet` and must not use the default value in `env.sh`.
>- The ORC header file `orc/io/InputStream.hh` references the sibling `orc/Adaptor.hh` using `#include "Adaptor.hh"`. If `-I/usr/local/include/orc` is missing, the error `fatal error: Adaptor.hh: No such file or directory` is reported.

### Build the C++ Plugin<a name="gluten_run_cpp_build"></a>

```shell
cd /opt/omni/src/Gluten
bash ./cpp-omni/build.sh
```

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- When `build.sh` is run without parameters, it builds in Release mode. If `FMT_HOME` and `FOLLY_HOME` are not set, it automatically clones the source code from `gitee.com` to build fmt 10.1.1 and folly v2024.07.01.00, and installs them to `/usr/local`.
>- If fmt and folly have already been installed, run `export FMT_HOME=/usr/local FOLLY_HOME=/usr/local` before the build to skip the repeated build.
>- When the build succeeds, the last output is `[100%] Built target spark_columnar_plugin`.

### Verify the C++ Plugin<a name="gluten_verify_cpp"></a>

```shell
ls -lh cpp-omni/build/releases/libspark_columnar_plugin.so
ldd cpp-omni/build/releases/libspark_columnar_plugin.so | grep 'not found'   # 无输出即为正常
```

The size of a normal artifact is about 5 MB. The key dependency resolution result is as follows.

```output
libprotobuf.so.32                          -> /opt/Gluten/lib/libprotobuf.so.32
libre2.so.11                               -> /usr/local/lib64/libre2.so.11
libfmt.so.10                               -> /usr/local/lib64/libfmt.so.10
libboostkit-omniop-vector-2.2.0-aarch64.so -> /opt/omni/install_internet/lib/...
libboostkit-omniop-operator-2.2.0-aarch64.so -> /opt/omni/install_internet/lib/...
```

### Build the Custom Arrow Java Package<a name="gluten_build_arrow_java"></a>

The `gluten-arrow` module depends on `org.apache.arrow:arrow-*:15.0.0-gluten`. This custom version has not been released to any public Maven repository, so it must be built from the Apache Arrow 15.0.0 source code with the patches provided in the Gluten repository and then installed to the local repository.

>![](public_sys-resources/icon-notice.gif) **NOTICE:**
>This section builds only the Java part of Arrow and does not build Arrow C++. It is unrelated to Velox. If this step is skipped, the `mvn` phase reports `Could not resolve dependencies ... org.apache.arrow:arrow-vector:jar:15.0.0-gluten`.

1. Download and decompress the Arrow 15.0.0 source code.

    ```shell
    cd /opt/omni/downloads
    curl -fsSL -o apache-arrow-15.0.0.tar.gz \
      https://mirrors.huaweicloud.com/apache/arrow/arrow-15.0.0/apache-arrow-15.0.0.tar.gz

    rm -rf /opt/omni/src/arrow-15.0.0 && mkdir -p /opt/omni/src/arrow-15.0.0
    tar -xzf apache-arrow-15.0.0.tar.gz -C /opt/omni/src/arrow-15.0.0 --strip-components=1
    ```

2. Apply the Arrow patches in the Gluten repository.

    ```shell
    cd /opt/omni/src/arrow-15.0.0
    patch -p1 < /opt/omni/src/Gluten/ep/build-velox/src/modify_arrow_dataset_scan_option.patch
    patch -p1 < /opt/omni/src/Gluten/ep/build-velox/src/modify_arrow.patch
    ```

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >`modify_arrow_dataset_scan_option.patch` adds classes such as `org.apache.arrow.dataset.scanner.FragmentScanOptions`. If the patch is not applied, the `gluten-arrow` build reports `object FragmentScanOptions is not a member of package org.apache.arrow.dataset.scanner`.

3. Change the version number and install the package to the local Maven repository.

    ```shell
    cd /opt/omni/src/arrow-15.0.0/java
    mvn -B versions:set -DnewVersion=15.0.0-gluten -DprocessAllModules -DgenerateBackupPoms=false

    mvn -B clean install -P arrow-c-data,arrow-jni \
      -pl bom,maven/module-info-compiler-maven-plugin,vector,c,dataset -am \
      -DskipTests -Drat.skip=true -Dmaven.gitcommitid.skip=true \
      -Dcheckstyle.skip=true -Dmaven.javadoc.skip=true
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The `c` and `dataset` modules are in the `arrow-c-data` and `arrow-jni` profiles and must be explicitly activated. Only JAR packages need to be generated, so the JNI native library build step in `-Parrow-jni` is not required.

4. Verify the artifacts in the local repository.

    ```shell
    ls /root/.m2/repository/org/apache/arrow/*/15.0.0-gluten/*.jar
    unzip -l /root/.m2/repository/org/apache/arrow/arrow-dataset/15.0.0-gluten/arrow-dataset-15.0.0-gluten.jar \
      | grep FragmentScanOptions
    ```

    If the JAR packages `arrow-vector`, `arrow-memory-core`, `arrow-memory-unsafe`, `arrow-c-data`, `arrow-dataset`, and others are present and `arrow-dataset` contains `FragmentScanOptions.class`, the result is normal.

### Build the Java/Scala Plugin Package<a name="gluten_build_java"></a>

1. Load the environment variables and run packaging.

    ```shell
    source /opt/omni/gluten_env.sh
    cd /opt/omni/src/Gluten

    mvn clean package -Pbackends-omni -Pspark-3.5 -DskipTests \
      -Dspotless.check.skip=true -Dscalastyle.skip=true -Dcheckstyle.skip=true
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >- `-Pbackends-omni` activates the Omni backend modules (`gluten-arrow` and `backends-omni`), and `-Pspark-3.5` specifies Spark 3.5.2.
    >- JDK 8 must be used. The first execution downloads all dependencies from the Huawei Cloud mirror and takes a long time.
    >- When the build succeeds, the last output is `BUILD SUCCESS`.

2. View the packaging result.

    ```shell
    ls -lh package/target/gluten-omni-bundle-*.jar
    ```

### Verify the Gluten Build Results<a name="gluten_verify_build"></a>

1. Check the artifacts.

    ```text
    /opt/omni/src/Gluten/
    ├── cpp-omni/build/releases/libspark_columnar_plugin.so   5.0M
    ├── backends-omni/target/backends-omni-1.3.0-3.5.jar      1.5M
    └── package/target/
        ├── gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar   28M
        └── gluten-package-1.3.0.jar
    ```

    The `openEuler_24.03_aarch_64` part of the JAR package name is automatically generated based on the OS and architecture of the build machine and must be consistent with the target runtime environment.

2. Check the dynamic library dependencies of the C++ plugin.

    ```shell
    ldd /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so | grep 'not found'
    ```

    No output indicates that the dependencies are resolved. If `libfmt.so.10` or `libboostkit-omniop-*` cannot be found, `ldconfig` has not taken effect. Perform [Configure Gluten Runtime Library Paths](#gluten_configure_ld_so) again and retry.

### Gluten Build Notes<a name="gluten_build_notes"></a>

- `cpp-omni/build.sh` deletes the `cpp-omni/build` directory and re-runs CMake configuration each time it is executed, so the preceding environment variables must take effect again in every build.
- The resource directory in `backends-omni/pom.xml` references `${cpp.releases.dir}`, which is defined only in `backends-velox`. Therefore, `libspark_columnar_plugin.so` is not packaged into `gluten-omni-bundle` and must be deployed separately together with the `lib` directory of OmniOperator.
- `cpp-omni` links OmniOperator with `-lboostkit-omniop-{vector,operator,codegen,reader}`. After OmniOperator is rebuilt, the C++ plugin must be rebuilt.
- During the entire build, do not run any velox-related script in the `dev/` directory. The dependency preparation in this section (fmt, folly, and Arrow Java) is completed by `cpp-omni/build.sh` and Maven.

## Create Release Packages<a name="package_release"></a>

After OmniOperator and Gluten are built, the artifacts in [Verify the Build Results](#verify_build) and [Verify the Gluten Build Results](#gluten_verify_build) can be packaged into the release packages required by Table 3 in the [Installation Guide](installation_guide.md), so that the build artifacts can be directly used for installation and deployment.

The packaging environment is the same as the build environment: openEuler 24.03 LTS aarch64, OmniOperator internet_commercial branch 2.2.0, Gluten internet_commercial branch 1.3.0, and Spark 3.5.2. All commands in this chapter are executed by the root user by default, and the packaging directory is `/opt/omni/release`.

**Table 2** Release Package List<a id="package_list"></a>

|Package|Item in the Installation Guide|Main content|
|--|--|--|
|BoostKit-omniop_2.2.0.zip|Item 1 in Table 3 (inner package of the OmniRuntime package)|OmniOperator operator libraries, header files, Java binding JAR packages, UDF JAR packages, and libboundscheck.so|
|BoostKit-omniruntime-gluten-2.2.0.zip|Item 4 in Table 3 (Gluten)|OmniOperator operator libraries, header files, Java binding, Gluten C++ plugin, Gluten plugin JAR package, and third-party libraries required by the plugin at runtime|
|Dependency_library_Gluten.zip|Item 4 in Table 3 (Gluten)|libLLVM-15.so and libjemalloc.so.2|
|Dependency_library_openeuler24.03.zip|Item 5 in Table 3 (Dependency_library category; new openEuler 24.03 package in this branch, with no identically named entry in the Installation Guide)|libLLVM-15.so and libjemalloc.so.2|

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- `BoostKit-omniruntime_1.9.0.zip` is the outer package of the OmniRuntime product. In addition to the OmniOperator installation package, it contains the closed-source component `BoostKit-omniadvisor_core_2.0.0.zip`, the license agreement, and version files, and cannot be produced by source code build. This chapter produces only the inner `BoostKit-omniop_2.2.0.zip`, which can be fully reproduced from the build artifacts.
>- LLVM and jemalloc must be built on the target OS to run properly. Therefore, they are not included in the OmniOperator installation package and are released separately as the Dependency_library package.
>- The binaries in the package are bound to the build environment. Artifacts built on openEuler 24.03 cannot be directly used on CentOS or openEuler 20.03/22.03.

### Pre-Packaging Checks<a name="package_preparation"></a>

The packaging inputs come from the [Build OmniOperator](#build_omnioperator) and [Build the Gluten Omni Backend](#build_gluten) stages. Before performing the operations, confirm that the files in [**Table 3**](#package_inputs) have been generated.

**Table 3** Packaging inputs<a id="package_inputs"></a>

|Category|Source path|Description|
|--|--|--|
|OmniOperator operator libraries|`/opt/omni/install_internet/lib/libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|The four types of operator libraries linked by the Gluten plugin|
|OmniOperator header files|`/opt/omni/install_internet/lib/include`|Used together with secondary development and deployment|
|Java binding library|`/opt/omni/install_internet/java-binding/libboostkit-omniop-java-binding-2.2.0-aarch64.so`|JNI library for calling OmniOperator from the Java side|
|Java binding JAR package|`/opt/omni/src/OmniOperator/bindings/java/target/boostkit-omniop-bindings-2.2.0-aarch64.jar`|Java calling package that contains the preceding JNI library|
|UDF JAR package|`/opt/omni/src/OmniOperator/core/src/udf/java/target/boostkit-omniop-udf-2.2.0-aarch64.jar`|JAR package used by the UDF plugin|
|securec implementation|`/usr/local/lib/libboundscheck.so`|Open-source securec implementation linked by the operator libraries with `-lboundscheck`|
|Gluten C++ plugin|`/opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so`|Artifact of `cpp-omni/build.sh`|
|Gluten plugin JAR package|`/opt/omni/src/Gluten/package/target/gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar`|Artifact of `mvn package`|
|Third-party runtime libraries|Protobuf, Arrow, ORC, HDFS, fmt, re2, folly, boundscheck, ABSL, and others|Installed to `/opt/Gluten/lib`, `/usr/local/lib`, and `/usr/local/lib64` when the dependencies are built|
|LLVM and jemalloc|`/usr/lib/llvm-15/lib/libLLVM-15.so`, `/usr/local/lib/libjemalloc.so.2`|Packaged separately as Dependency_library packages|

1. Check whether the files in the preceding table exist.

    ```shell
    ls -lh /opt/omni/install_internet/lib /opt/omni/install_internet/java-binding
    ls -lh /usr/local/lib/libboundscheck.so
    ls -lh /opt/omni/src/OmniOperator/bindings/java/target/*-aarch64.jar
    ls -lh /opt/omni/src/OmniOperator/core/src/udf/java/target/*-aarch64.jar
    ls -lh /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so
    ls -lh /opt/omni/src/Gluten/package/target/gluten-omni-bundle-*.jar
    ```

2. Create the packaging directory.

    ```shell
    mkdir -p /opt/omni/release/stage
    ```

### Collect the Runtime Dependency List<a name="collect_runtime_dependencies"></a>

The plugin and operator libraries depend on a large number of third-party libraries, which are easy to miss if listed one by one. Here, the dependency closure is recursively resolved using `ldd`, and all library files outside the system directories are automatically collected as the basis for packaging.

1. Create the dependency collection script `/opt/omni/collect_runtime_deps.sh`.

    ```shell
    cat > /opt/omni/collect_runtime_deps.sh <<'EOF'
    #!/bin/bash
    # 递归解析运行时依赖闭包（排除系统目录：/lib64 /usr/lib64 /usr/lib /lib）
    set -u
    SEEDS="/opt/omni/install_internet/lib/libboostkit-omniop-vector-2.2.0-aarch64.so
    /opt/omni/install_internet/lib/libboostkit-omniop-operator-2.2.0-aarch64.so
    /opt/omni/install_internet/lib/libboostkit-omniop-codegen-2.2.0-aarch64.so
    /opt/omni/install_internet/lib/libboostkit-omniop-reader-2.2.0-aarch64.so
    /opt/omni/install_internet/java-binding/libboostkit-omniop-java-binding-2.2.0-aarch64.so
    /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so"

    QUEUE=$(mktemp)
    SEEN=$(mktemp)
    echo "$SEEDS" > "$QUEUE"

    while read -r lib; do
      [ -z "$lib" ] && continue
      grep -qxF "$lib" "$SEEN" && continue
      echo "$lib" >> "$SEEN"
      ldd "$lib" 2>/dev/null | awk '/=>/ {print $3} /^\t\// {print $1}' | grep '^/' | while read -r dep; do
        case "$dep" in
          /lib64/*|/usr/lib64/*|/usr/lib/*|/lib/*) ;;
          *) echo "$dep" >> "$QUEUE" ;;
        esac
      done
    done < "$QUEUE"

    sort -u "$QUEUE" > /tmp/gluten_closure.txt
    cat /tmp/gluten_closure.txt
    echo "total: $(wc -l < /tmp/gluten_closure.txt)"
    EOF
    chmod +x /opt/omni/collect_runtime_deps.sh
    ```

2. Execute the script and view the result.

    ```shell
    bash /opt/omni/collect_runtime_deps.sh
    ```

    A total of 81 library files are collected in this build environment, covering Protobuf, Arrow, ORC, HDFS, fmt, re2, folly, boundscheck, ABSL, and the OmniOperator libraries.

### Create BoostKit-omniop_2.2.0.zip<a name="make_omniop_package"></a>

This package corresponds to Item 1 in Table 3 of the [Installation Guide](installation_guide.md) and is the inner installation package obtained after `BoostKit-omniruntime_1.9.0.zip` is decompressed. Its content corresponds one-to-one to the inputs in [**Table 3**](#package_inputs).

1. Assemble the package directory.

    ```shell
    PKG=/opt/omni/release/stage/boostkit-omniop-operator-2.2.0-aarch64
    rm -rf "$PKG" && mkdir -p "$PKG"

    cp -a /opt/omni/install_internet/lib/include "$PKG"/
    cp -L /opt/omni/install_internet/lib/libboostkit-omniop-vector-2.2.0-aarch64.so \
          /opt/omni/install_internet/lib/libboostkit-omniop-operator-2.2.0-aarch64.so \
          /opt/omni/install_internet/lib/libboostkit-omniop-codegen-2.2.0-aarch64.so \
          /opt/omni/install_internet/lib/libboostkit-omniop-reader-2.2.0-aarch64.so "$PKG"/
    cp -L /usr/local/lib/libboundscheck.so "$PKG"/
    cp -L /opt/omni/src/OmniOperator/bindings/java/target/boostkit-omniop-bindings-2.2.0-aarch64.jar \
          /opt/omni/src/OmniOperator/core/src/udf/java/target/boostkit-omniop-udf-2.2.0-aarch64.jar "$PKG"/
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >`boostkit-omniop-bindings-2.2.0-aarch64.jar` already contains `libboostkit-omniop-java-binding-2.2.0-aarch64.so`, which is consistent with the official package structure. Therefore, the .so file does not need to be copied separately.

2. Generate the tar.gz.

    ```shell
    cd /opt/omni/release/stage
    tar --owner root --group root -zcf boostkit-omniop-operator-2.2.0-aarch64.tar.gz boostkit-omniop-operator-2.2.0-aarch64
    ```

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >The tar.gz must retain the top-level directory `boostkit-omniop-operator-2.2.0-aarch64/`. The command `cd /opt/omni-operator/boostkit-omniop-operator-2.2.0-aarch64` in the Installation Guide depends on this directory level.

3. Compress to zip and calculate the checksum.

    ```shell
    cd /opt/omni/release/stage
    zip -q /opt/omni/release/BoostKit-omniop_2.2.0.zip boostkit-omniop-operator-2.2.0-aarch64.tar.gz
    sha256sum /opt/omni/release/BoostKit-omniop_2.2.0.zip
    ```

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >The OmniOperator source directory provides the packaging script `build.sh package`, whose internal procedure is the same as the assembly steps in this section: copy `$OMNI_HOME/lib`, copy the two `*-aarch64.jar` files, `tar`, and `zip`. To use this script, set `OMNI_HOME` and `JAVA_HOME`, and make sure that the `libboundscheck` and `json` directories exist at the same level as the source directory. The script rebuilds the entire project. If build artifacts already exist, assemble the package manually by following the steps in this section to avoid repeated builds.

### Create BoostKit-omniruntime-gluten-2.2.0.zip<a name="make_omniruntime_gluten_package"></a>

This package corresponds to the Gluten scenario in Item 4 in Table 3 of the [Installation Guide](installation_guide.md) and contains the OmniOperator artifacts, Gluten artifacts, and third-party libraries required by the plugin at runtime.

1. Create the package directory and copy the OmniOperator header files.

    ```shell
    PKG=/opt/omni/release/stage/BoostKit-omniruntime-gluten-2.2.0
    rm -rf "$PKG" && mkdir -p "$PKG"
    cd "$PKG"
    cp -a /opt/omni/install_internet/lib/include ./
    ```

2. Copy the library files in the dependency closure.

    >![](public_sys-resources/icon-notice.gif) **NOTICE:**
    >`libLLVM-15.so` and `libjemalloc.so.2` are not included in this package. They are provided by the Dependency_library package to avoid packing about 70 MB repeatedly.

    ```shell
    while read -r f; do
      case "$f" in
        */libLLVM-15.so|*/libjemalloc.so.2) continue ;;
      esac
      cp -L "$f" .
    done < /tmp/gluten_closure.txt
    ```

3. Copy the Gluten plugin and JAR package.

    ```shell
    cp -L /opt/omni/install_internet/java-binding/libboostkit-omniop-java-binding-2.2.0-aarch64.so .
    cp -L /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so .
    cp -L /opt/omni/src/Gluten/package/target/gluten-omni-bundle-*.jar .
    ```

4. Generate the README.txt file.

    ```shell
    cat > README.txt <<'EOF'
    BoostKit OmniRuntime Gluten 2.2.0
    =================================
    Applicable environment: openEuler 24.03 LTS aarch64, Gluten internet_commercial branch, Spark 3.5.2

    Directory description:
    - include/                                        OmniOperator header files
    - libboostkit-omniop-*.so                         OmniOperator operator libraries (vector/operator/codegen/reader)
    - libboostkit-omniop-java-binding-*.so            Java binding dynamic library
    - libspark_columnar_plugin.so                     Gluten C++ execution plugin
    - gluten-omni-bundle-*.jar                        Gluten Spark plugin package
    - libprotobuf/libarrow/liborc/libhdfs/libfmt etc. Third-party libraries required by the plugin at runtime

    For the installation method, see the Gluten scenario chapter of the Installation Guide: copy the preceding files to /opt/omni-operator/lib.
    LLVM-15 and jemalloc are provided by Dependency_library_Gluten.zip.
    EOF
    ```

5. Compress the release package.

    ```shell
    cd /opt/omni/release/stage
    zip -r -q /opt/omni/release/BoostKit-omniruntime-gluten-2.2.0.zip BoostKit-omniruntime-gluten-2.2.0
    ```

    The top-level directory `BoostKit-omniruntime-gluten-2.2.0/` is retained in the package, and decompressing the package produces the installation directory content.

### Create the Dependency_library Packages<a name="make_dependency_library_package"></a>

The Dependency_library package contains only LLVM and jemalloc, which must be built on the target OS. The two packages for the Gluten and OmniOperator scenarios have the same content and differ only in name.

1. Create the dependency package for the Gluten scenario.

    ```shell
    cd /opt/omni/release/stage
    mkdir -p Dependency_library_Gluten
    cp -L /usr/lib/llvm-15/lib/libLLVM-15.so Dependency_library_Gluten/
    cp -L /usr/local/lib/libjemalloc.so.2 Dependency_library_Gluten/
    zip -r -q /opt/omni/release/Dependency_library_Gluten.zip Dependency_library_Gluten
    ```

2. Create the dependency package named after the OS.

    ```shell
    cd /opt/omni/release/stage
    mkdir -p Dependency_library_openeuler24.03
    cp -L /usr/lib/llvm-15/lib/libLLVM-15.so Dependency_library_openeuler24.03/
    cp -L /usr/local/lib/libjemalloc.so.2 Dependency_library_openeuler24.03/
    zip -r -q /opt/omni/release/Dependency_library_openeuler24.03.zip Dependency_library_openeuler24.03
    ```

3. Calculate the checksums of all packages.

    ```shell
    cd /opt/omni/release
    sha256sum *.zip > SHA256SUM.txt
    cat SHA256SUM.txt
    ```

### Package Contents<a name="package_contents"></a>

**Table 4** Contents of BoostKit-omniop_2.2.0.zip<a id="omniop_package_contents"></a>

|Category|File|Quantity|Description|
|--|--|--|--|
|Header files|`include/` (`vector`, `util`, `codegen`, `operator`, and others)|262|OmniOperator development header files|
|Operator libraries|`libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|4|The operator libraries|
|securec implementation|`libboundscheck.so`|1|The operator libraries are linked with `-lboundscheck`|
|Java binding|`boostkit-omniop-bindings-2.2.0-aarch64.jar`|1|Contains `libboostkit-omniop-java-binding-2.2.0-aarch64.so`|
|UDF plugin|`boostkit-omniop-udf-2.2.0-aarch64.jar`|1|Used in the UDF scenario|

Packaging result: the package is about 19 MB, and the tar.gz contains 300 entries (including directories), which is consistent with the directory structure of the official `BoostKit-omniop_2.0.0.zip`.

**Table 5** Contents of BoostKit-omniruntime-gluten-2.2.0.zip<a id="boostkit_package_contents"></a>

|Category|File|Quantity|Description|
|--|--|--|--|
|Header files|`include/` (`vector`, `util`, `codegen`, `operator`, and others)|262|OmniOperator development header files|
|OmniOperator operator libraries|`libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|4|Linked at plugin build time and loaded at runtime|
|Java binding library|`libboostkit-omniop-java-binding-2.2.0-aarch64.so`|1|JNI call from the Java side|
|Gluten plugin|`libspark_columnar_plugin.so`|1|Entry point for Gluten execution plan pushdown|
|Gluten plugin package|`gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar`|1|Spark plugin package|
|Third-party runtime libraries|`libprotobuf.so.32`, `libarrow.so.1100`, `libarrow_dataset.so.1100`, `libparquet.so.1100`, `liborc.so`, `libhdfs.so.0.0.0`, `libfmt.so.10`, `libre2.so.11`, `libfolly.so.0.58.0-dev`, `libboundscheck.so`|10|Installed to `/opt/Gluten/lib`, `/usr/local/lib`, and `/usr/local/lib64` at build time|
|ABSL libraries|`libabsl_*.so.2407.0.0`|64|ABSL 20240722.0, required by Gluten at runtime|
|Readme file|`README.txt`|1|Version and directory description|

Packaging result: the package is about 105 MB, about 292 MB after decompression, with 375 files in total.

**Table 6** Contents of the Dependency_library packages<a id="dependency_package_contents"></a>

|File|Size|Description|
|--|--|--|
|`libLLVM-15.so`|67MB|Required by the `codegen` module at runtime|
|`libjemalloc.so.2`|5.5MB|Memory allocator, must be built on the target OS|

### Post-Packaging Verification<a name="package_verification"></a>

1. Check the package structure.

    ```shell
    cd /opt/omni/release
    unzip -l BoostKit-omniop_2.2.0.zip
    unzip -l BoostKit-omniruntime-gluten-2.2.0.zip | head -20
    unzip -l BoostKit-omniruntime-gluten-2.2.0.zip | tail -2
    unzip -l Dependency_library_Gluten.zip
    ```

2. Check whether all dependencies can be resolved after decompression.

    ```shell
    rm -rf /tmp/ziptest && mkdir -p /tmp/ziptest && cd /tmp/ziptest
    unzip -q /opt/omni/release/BoostKit-omniruntime-gluten-2.2.0.zip
    unzip -q /opt/omni/release/Dependency_library_Gluten.zip

    P=/tmp/ziptest/BoostKit-omniruntime-gluten-2.2.0
    cp Dependency_library_Gluten/libLLVM-15.so Dependency_library_Gluten/libjemalloc.so.2 "$P"/

    LD_LIBRARY_PATH="$P" ldd "$P/libspark_columnar_plugin.so" | grep 'not found'
    LD_LIBRARY_PATH="$P" ldd "$P/libboostkit-omniop-reader-2.2.0-aarch64.so" | grep 'not found'
    ```

    No output from both commands indicates that the dependencies of the plugin and operator libraries are all included in the package.

3. Verify the ABSL dependency versions of the Gluten plugin.

    ```shell
    LD_LIBRARY_PATH="$P" ldd "$P/libspark_columnar_plugin.so" | grep 'libabsl'
    ```

    All output library names must be `libabsl_*.so.2407.0.0`, and all resolved paths must point to the library files in `$P`. If another soname version or `not found` is displayed, the ABSL version in the package does not match the plugin, and the ABSL build and installation step in the [Installation Guide](installation_guide.md) cannot be skipped.

### Using the Packages<a name="package_to_installation"></a>

The release packages can be directly used in the installation process described in the [Installation Guide](installation_guide.md): use `BoostKit-omniruntime-gluten-2.2.0.zip` and `Dependency_library_Gluten.zip` for the Gluten scenario. The OmniOperator core operator libraries and runtime dependencies are provided by `BoostKit-omniop_2.2.0.zip` and the `Dependency_library` package for the corresponding OS, respectively.

>![](public_sys-resources/icon-note.gif) **NOTE:**
>
>- The Gluten package has 64 built-in ABSL libraries (20240722.0). The manual ABSL build step in the Gluten scenario of the [Installation Guide](installation_guide.md) can be skipped only when all ABSL dependencies of the plugin are resolved to `libabsl_*.so.2407.0.0` in the package during [Post-Packaging Verification](#package_verification). Otherwise, build and install the ABSL version required by the plugin as described in the Installation Guide.
>- The dependency installation commands in the Installation Guide use openEuler 22.03 as an example. When using the dependency package generated by this guide on openEuler 24.03, replace `Dependency_library_openeuler22.03.zip` and the extracted directory `Dependency_library_openeuler22.03` in the commands with `Dependency_library_openeuler24.03.zip` and `Dependency_library_openeuler24.03`, respectively. Otherwise, the commands fail because the file or directory does not exist.
>- `BoostKit-omniop_2.2.0.zip` contains only the OmniOperator artifacts and does not contain third-party libraries such as folly, protobuf, orc, and arrow. The official 2.0.0 statically links these dependencies into the operator libraries, whereas this branch uses dynamic linking. The target machine must also provide these libraries during deployment.
>- This branch uses `libboundscheck.so` instead of the `libsecurec.so` in the official package. When running the copy commands in the Installation Guide, use the actual file names in the package.

## Deployment Notes<a name="deployment_notes"></a>

- `/opt/omni/install_internet/lib` is the library directory used for deployment. `libboundscheck.so`, `liborc.so`, `libfolly.so`, and `libarrow*` are external dependencies and are not included in this directory. Install them on the target machine or add them to `LD_LIBRARY_PATH`.
- If the target machine does not have the `/opt/Adaptor` directory, copy `libhdfs.so*`, `libhdfspp.so*`, and `include/` to the target machine.
- The `libspark_columnar_plugin.so` and `gluten-omni-bundle-*.jar` of the Gluten Omni backend are located in `/opt/omni/src/Gluten` rather than `/opt/omni/install_internet`, and must be deployed together with the OmniOperator `lib` directory.
- The version information is subject to the verification output in the [Verify the Build Results](#verify_build) section.
