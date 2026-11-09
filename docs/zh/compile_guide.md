# OmniOperator源码编译指南<a name="source_build_guide"></a>

本文介绍OmniOperator及Gluten Omni后端的源码编译流程，包括环境准备、依赖安装、编译产物校验和发布包制作。

## 编译简介<a name="omni_build_introduction"></a>

本文介绍在openEuler 24.03 LTS aarch64环境下，从源码编译internet_commercial分支OmniOperator的完整操作步骤，按环境准备、源码包准备、依赖安装、编译出包、编译结果验证和制作发布包的顺序组织。开始操作前，请先参见[**表 1** 环境要求](#build_environment_requirements)完成环境检查，再依次执行后续章节的操作。

本文所有命令默认在root用户下执行，工作目录统一为`/opt/omni`。

OmniOperator编译完成后，如需使用Gluten Omni后端，可继续执行[编译Gluten Omni后端](#build_gluten)章节。

**表 1** 环境要求<a id="build_environment_requirements"></a>

|项目|要求|
|--|--|
|操作系统|openEuler 24.03 LTS（基线版本及SP1、SP2等后续补丁版本均可，无特定SP版本要求）|
|处理器架构|aarch64|
|用户权限|root|
|硬件资源|建议32核及以上CPU、64GB及以上内存、40GB及以上可用磁盘空间|
|网络|可访问`repo.openeuler.org`、`gitcode.com`、`gitee.com`、`github.com`（无法直连时需配置可用代理）、`mirrors.huaweicloud.com`及所使用的Maven镜像|
|本地待上传文件|无，全部源码在服务器上直接下载或克隆|

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- 本文在openEuler 24.03 LTS基线版本上验证通过。编译步骤不依赖特定SP版本，[克隆补丁仓库](#clone_patch_repos)章节克隆的`openEuler-24.03-LTS-SP1-release`分支仅用于获取固定的源码快照，与系统实际SP版本无关。
>- 下文源码包默认从GitHub直接下载。若GitHub无法直连，可将[下载源码包](#download_source_packages)章节中的`GH`变量替换为所用代理的前缀地址（格式为`https://<代理域名>/https://github.com`）。

## 系统准备<a name="system_preparation"></a>

### 安装编译工具链<a name="install_toolchain"></a>

1. 安装基础编译工具链。

    ```shell
    dnf install -y git gcc gcc-c++ make cmake ninja-build autoconf automake libtool m4 \
      perl python3 curl wget tar gzip bzip2 xz unzip zip file which findutils procps-ng \
      util-linux patch openssl openssl-devel zlib-devel libffi-devel ncurses-devel krb5-devel \
      java-1.8.0-openjdk-devel
    ```

2. 安装Arrow编译所需的压缩库开发头文件。

    ```shell
    dnf install -y utf8proc-devel libcurl-devel boost-devel bison flex \
      brotli-devel bzip2-devel lz4-devel snappy-devel libzstd-devel
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >Arrow编译时依赖上述压缩库的开发头文件，任意一个缺失都会导致CMake阶段报错找不到依赖。

3. 安装cyrus-sasl-devel。

    ```shell
    dnf install -y cyrus-sasl-devel
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >Hadoop的libhdfs++编译时需要查找SASL库，缺少cyrus-sasl-devel会报“Could not find a SASL library (GSASL (gsasl) or Cyrus SASL (libsasl2)”。

4. 验证编译工具链是否安装成功。

    ```shell
    java -version          # 版本为1.8.0_462
    mvn -v 2>/dev/null     # Maven尚未安装，请参见安装Maven章节
    ninja --version
    ```

### 创建目录<a name="create_directories"></a>

1. 创建编译过程中使用的目录。

    ```shell
    mkdir -p /opt/omni/{src,downloads,logs,tools,adaptor}
    mkdir -p /opt/omni/downloads/arrow-thirdparty
    ```

2. 创建`/opt/Adaptor`软链接，供后续Hadoop安装与编译时统一引用。

    ```shell
    ln -sfn /opt/omni/adaptor /opt/Adaptor
    ```

### 配置动态库搜索路径<a name="configure_ld_so"></a>

openEuler默认的`/etc/ld.so.conf`不包含`/usr/local/lib`与`/usr/local/lib64`，而本文安装的依赖均位于这两个目录，需要手动加入动态库搜索路径。

```shell
cat > /etc/ld.so.conf.d/usr-local.conf <<'EOF'
/usr/local/lib
/usr/local/lib64
EOF
ldconfig
```

### 安装Maven<a name="install_maven"></a>

1. 下载并解压Maven 3.8.3。

    ```shell
    curl -sSL -o /opt/omni/downloads/apache-maven-3.8.3-bin.tar.gz \
      https://mirrors.huaweicloud.com/apache/maven/maven-3/3.8.3/binaries/apache-maven-3.8.3-bin.tar.gz
    tar -xzf /opt/omni/downloads/apache-maven-3.8.3-bin.tar.gz -C /opt/omni/tools
    ```

2. 在`/root/.m2/settings.xml`中配置华为云中央仓镜像。

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

### 生成环境变量脚本<a name="configure_env_script"></a>

1. 创建环境变量脚本`/opt/omni/env.sh`。

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

2. 赋予脚本执行权限并使其生效。

    ```shell
    chmod +x /opt/omni/env.sh
    source /opt/omni/env.sh
    ```

>![须知](public_sys-resources/icon-notice.gif) **须知：**
>`JAVA_HOME`必须使用`/usr/lib/jvm/java-1.8.0-openjdk`这个alternatives软链接，不能写成带构建号的真实目录。

### 克隆补丁仓库<a name="clone_patch_repos"></a>

1. 克隆补丁仓库与snappy源码仓库。

    ```shell
    cd /opt/omni/src
    git clone --depth 1 --branch main https://gitee.com/kunpengcompute/boostkit-bigdata.git
    git clone --depth 1 --branch openEuler-24.03-LTS-SP1-release https://gitee.com/src-openeuler/snappy.git snappy-src
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >`boostkit-bigdata`提供Arrow与Hadoop的补丁，`snappy-src`提供snappy源码与两个补丁。

2. 离线环境处理。在可联网机器上执行上述克隆命令，将仓库打包后拷贝至`/opt/omni/src`目录解压。

## 准备源码包<a name="prepare_source_packages"></a>

### 下载源码包<a name="download_source_packages"></a>

1. 下载全部源码包。

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

    # Arrow构建期使用的第三方
    get "$GH/microsoft/mimalloc/archive/refs/tags/v2.0.6.tar.gz"                       "$TP/mimalloc-v2.0.6.tar.gz"
    get "$GH/substrait-io/substrait/archive/refs/tags/v0.20.0.tar.gz"                  "$TP/substrait-v0.20.0.tar.gz"
    get "$GH/google/re2/archive/refs/tags/2022-06-01.tar.gz"                           "$TP/re2-2022-06-01.tar.gz"
    get "$GH/apache/thrift/archive/refs/tags/v0.16.0.tar.gz"                           "$TP/thrift-0.16.0.tar.gz"
    get "$GH/xtensor-stack/xsimd/archive/refs/tags/9.0.1.tar.gz"                       "$TP/xsimd-9.0.1.tar.gz"
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >thrift的tag带有`v`前缀，写成`0.16.0`会返回404，归档包的顶层目录仍为`thrift-0.16.0`。

2. 检查下载结果，确认文件大小与完整性。下载失败的文件会打印“FAILED”。

    ```shell
    ls -l $DL/*.tar.* $DL/*.xz $TP/*.tar.gz
    ```

### 重新打包Protobuf v21.3<a name="repack_protobuf"></a>

Arrow的ExternalProject要求归档包的顶层目录为`protobuf-21.3`，因此需要从GitHub克隆v21.3分支后重新打包。

```shell
cd /opt/omni/src
rm -rf protobuf-21.3
git clone --depth 1 -b v21.3 https://github.com/protocolbuffers/protobuf.git protobuf-21.3

tar -czf /opt/omni/downloads/arrow-thirdparty/protobuf-v21.3.tar.gz protobuf-21.3
sha256sum /opt/omni/downloads/arrow-thirdparty/protobuf-v21.3.tar.gz
```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>克隆目录命名为`protobuf-21.3`，打包后的顶层目录正好满足Arrow的命名要求，[安装Arrow](#install_arrow)章节会据此重新计算校验值并写入`versions.txt`。
>![须知](public_sys-resources/icon-notice.gif) **须知：**
>此处的Protobuf v21.3仅用于Arrow内部构建，系统中安装的Protobuf 3.21.9（请参见[安装Protobuf](#install_protobuf)）不要与之混用。

### 重新打包jemalloc 5.3.0<a name="repack_jemalloc"></a>

GitHub归档包不包含`configure`脚本，且Arrow的ExternalProject不会自动执行`autogen.sh`，需要先生成`configure`再重新打包。

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

### 获取ORC源码<a name="obtain_orc"></a>

ORC源码已托管至[公开仓库](https://gitcode.com/boostkit/orc)，直接克隆`v1.7.4_new`分支即可。

```shell
cd /opt/omni/src
rm -rf orc
git clone -b v1.7.4_new https://gitcode.com/boostkit/orc.git orc
```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>克隆后`orc`仓库的顶层目录即为ORC源码根目录（包含`CMakeLists.txt`、`c++`、`proto`、`tools`等），[安装ORC](#install_orc)章节将在`/opt/omni/src/orc`目录下编译。

## 安装公共依赖<a name="install_common_dependencies"></a>

本章节介绍编译OmniOperator所需的公共依赖安装步骤，请按顺序依次执行。

### 安装Protobuf 3.21.9<a name="install_protobuf"></a>

Hadoop的libhdfspp、ORC以及Maven的protoc插件均依赖Protobuf，安装至`/opt/Gluten`目录。

1. 编译并安装Protobuf。

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

2. 验证Protobuf是否安装成功。

    ```shell
    /opt/Gluten/bin/protoc --version    # 版本为libprotoc 3.21.9
    ```

### 安装Arrow 11.0.0<a name="install_arrow"></a>

1. 解压源码并应用补丁。

    ```shell
    cd /opt/omni/src
    rm -rf arrow && mkdir arrow
    tar -xzf /opt/omni/downloads/arrow-apache-arrow-11.0.0.tar.gz -C arrow --strip-components=1

    cp boostkit-bigdata/omnioperator/contrib/arrow-maint-11_0_0/arrow-maint-11_0_0.patch arrow/
    cd arrow
    patch -p1 < arrow-maint-11_0_0.patch
    ```

2. 将第三方包指向本地文件，并回写校验值。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >mimalloc、protobuf、jemalloc等第三方包为本地重新打包，其SHA256值与官方发布的校验值不一致，需要重新计算并写入`cpp/thirdparty/versions.txt`，否则编译会直接失败。

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

3. 编译并安装Arrow。

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`ARROW_DATASET`必须设置为`ON`，`reader`模块依赖`ArrowDataset`。

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

4. 验证Arrow是否安装成功。

    ```shell
    grep ARROW_VERSION_STRING /usr/local/include/arrow/util/config.h   # "11.0.0"
    ls /usr/local/lib64/cmake/ArrowDataset
    ```

### 安装nlohmann/json 3.11.3<a name="install_json"></a>

```shell
cd /opt/omni/src
rm -rf json && mkdir json
tar -xzf /opt/omni/downloads/json-v3.11.3.tar.gz -C json --strip-components=1
cmake -S json -B json/build -DCMAKE_BUILD_TYPE=Release -DJSON_BuildTests=OFF -DJSON_Install=ON
cmake --build json/build --parallel 32
cmake --install json/build
```

### 安装jemalloc 5.3.0<a name="install_jemalloc"></a>

```shell
cd /opt/omni/src
rm -rf jemalloc && mkdir jemalloc
tar -xzf /opt/omni/downloads/arrow-thirdparty/jemalloc-5.3.0.tar.gz -C jemalloc --strip-components=1
cd jemalloc
./autogen.sh --disable-initial-exec-tls --with-lg-page=16
make -j32
make install
```

验证jemalloc是否安装成功。执行`ldconfig -p | grep jemalloc`可查询到该库，头文件位于`/usr/local/include/jemalloc/jemalloc.h`。

### 安装zlib 1.3.1<a name="install_zlib"></a>

```shell
cd /opt/omni/src
rm -rf zlib && mkdir zlib
tar -xzf /opt/omni/downloads/zlib-v1.3.1.tar.gz -C zlib --strip-components=1
cd zlib
./configure --prefix=/usr/local
make -j32
make install
```

### 安装LZ4 1.10.0<a name="install_lz4"></a>

```shell
cd /opt/omni/src
rm -rf lz4 && mkdir lz4
tar -xzf /opt/omni/downloads/lz4-v1.10.0.tar.gz -C lz4 --strip-components=1
cd lz4
make -j32
make install PREFIX=/usr/local
```

### 安装Snappy 1.1.10<a name="install_snappy"></a>

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

### 安装Zstandard 1.5.6<a name="install_zstd"></a>

```shell
cd /opt/omni/src
rm -rf zstd && mkdir zstd
tar -xzf /opt/omni/downloads/zstd-v1.5.6.tar.gz -C zstd --strip-components=1
cd zstd
make -j32 PREFIX=/usr/local
make install PREFIX=/usr/local
```

### 安装Cyrus SASL 2.1.28<a name="install_cyrus_sasl"></a>

```shell
cd /opt/omni/src
rm -rf cyrus-sasl && mkdir cyrus-sasl
tar -xzf /opt/omni/downloads/cyrus-sasl-2.1.28.tar.gz -C cyrus-sasl --strip-components=1
cd cyrus-sasl
./autogen.sh
make -j32
make install
```

### 安装zlib-ng 2.0.4<a name="install_zlib_ng"></a>

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

### 安装jsoncpp 1.9.6<a name="install_jsoncpp"></a>

```shell
cd /opt/omni/src
rm -rf jsoncpp && mkdir jsoncpp
tar -xzf /opt/omni/downloads/jsoncpp-1.9.6.tar.gz -C jsoncpp --strip-components=1
rm -rf jsoncpp/build && mkdir -p jsoncpp/build && cd jsoncpp/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF
make -j32
make install
```

### 安装Hadoop 3.2.0<a name="install_hadoop"></a>

1. 解压源码并应用补丁。

    ```shell
    source /opt/omni/env.sh

    cd /opt/omni/src
    rm -rf hadoop && mkdir hadoop
    tar -xzf /opt/omni/downloads/hadoop-rel-release-3.2.0.tar.gz -C hadoop --strip-components=1

    cp boostkit-bigdata/omnioperator/contrib/hadoop-3_2_0/hadoop-3_2_0.patch hadoop/
    cd hadoop
    patch -p1 < hadoop-3_2_0.patch
    ```

2. 编译Hadoop native部分。

    ```shell
    cd /opt/omni/src/hadoop/hadoop-hdfs-project/hadoop-hdfs-native-client
    mvn clean package -DskipTests -Pdist,native -Dtar
    ```

3. 安装头文件与库到`/opt/Adaptor`。

    ```shell
    cd /opt/omni/src/hadoop/hadoop-hdfs-project/hadoop-hdfs-native-client
    mkdir -p /opt/Adaptor/include
    cp -rf src/main/native/libhdfs/include/* /opt/Adaptor/include/
    cp target/hadoop-hdfs-native-client-3.2.0/include/hdfs.h /opt/Adaptor/include/   # 若不存在，使用 target/target/usr/local/include/hdfs.h
    cp -rf src/main/native/libhdfspp/include /opt/Adaptor/
    cp -rf target/target/usr/local/lib /opt/Adaptor/
    cp -f target/main/native/libhdfspp/libhdfspp.so* /opt/Adaptor/lib/
    ```

4. 验证Hadoop是否安装成功。

    ```shell
    ls /opt/Adaptor/lib       # libhdfs.so.0.0.0、libhdfspp.so.0.1.0
    ls /opt/Adaptor/include   # hdfs  hdfs.h  hdfspp
    ```

### 安装GoogleTest 1.14.0<a name="install_googletest"></a>

```shell
cd /opt/omni/src
rm -rf googletest && mkdir googletest
tar -xzf /opt/omni/downloads/googletest-v1.14.0.tar.gz -C googletest --strip-components=1
cmake -S googletest -B googletest/build -DCMAKE_BUILD_TYPE=Release
cmake --build googletest/build --parallel 32
cmake --install googletest/build
```

### 安装Abseil-cpp 20240722.0<a name="install_abseil"></a>

```shell
cd /opt/omni/src
rm -rf abseil-cpp && mkdir abseil-cpp
tar -xzf /opt/omni/downloads/abseil-cpp-20240722.0.tar.gz -C abseil-cpp --strip-components=1
cmake -S abseil-cpp -B abseil-cpp/build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build abseil-cpp/build --parallel 32
cmake --install abseil-cpp/build
```

### 安装RE2 2024-07-02<a name="install_re2"></a>

```shell
cd /opt/omni/src
rm -rf re2 && mkdir re2
tar -xzf /opt/omni/downloads/re2-2024-07-02.tar.gz -C re2 --strip-components=1
cmake -S re2 -B re2/build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_PREFIX_PATH=/usr/local
cmake --build re2/build --parallel 32
cmake --install re2/build
```

### 安装libboundscheck<a name="install_libboundscheck"></a>

`vector`、`util`、`codegen`均以`-lboundscheck`链接，源码的include路径为`<libboundscheck/include/securec.h>`，安装时头文件目录层级必须保持不变。

1. 编译并安装libboundscheck。

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

2. 验证libboundscheck是否安装成功。

    ```shell
    ls /usr/local/lib/libboundscheck.so
    ls /usr/local/include/libboundscheck/include/securec.h
    ```

### 安装RapidJSON 1.1.0<a name="install_rapidjson"></a>

RapidJSON为纯头文件库，直接拷贝头文件即可。

```shell
cd /opt/omni/src
tar -zxf /opt/omni/downloads/rapidjson-v1.1.0.tar.gz -C /opt/omni/src
mkdir -p /usr/local/include
cp -a /opt/omni/src/rapidjson-1.1.0/include/rapidjson /usr/local/include/
```

验证RapidJSON是否安装成功：`/usr/local/include/rapidjson/document.h`文件存在即为正常。

### 安装LLVM/Clang 15.0.4<a name="install_llvm"></a>

`codegen`模块硬编码`/usr/lib/llvm-15/include`目录，并链接`-lLLVM-15`，必须按该路径安装且生成动态库。

1. 编译并安装LLVM/Clang。

    ```shell
    cd /opt/omni/src
    tar -xf /opt/omni/downloads/llvm-project-15.0.4.src.tar.xz
    # 顶层目录为 llvm-project-15.0.4.src

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

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`LLVM_ENABLE_RTTI=ON`与`LLVM_BUILD_LLVM_DYLIB=ON`缺一不可，否则codegen编译或链接会失败。

2. 配置动态库路径与clang++可用的符号链接。

    ```shell
    ln -sf /usr/lib/llvm-15/lib/libLLVM-15.so /usr/local/lib64/libLLVM-15.so

    echo /usr/lib/llvm-15/lib > /etc/ld.so.conf.d/llvm-15.conf
    ldconfig

    for b in clang clang++ llvm-config llvm-ar llvm-link opt llc llvm-as; do
      ln -sf /usr/lib/llvm-15/bin/$b /usr/local/bin/$b
    done

    # openEuler的GCC triple为aarch64-openEuler-linux，clang 15默认查找aarch64-unknown-linux-gnu
    ln -sfn /usr/lib/gcc/aarch64-openEuler-linux /usr/lib/gcc/aarch64-unknown-linux-gnu
    ln -sfn /usr/include/c++/12/aarch64-openEuler-linux /usr/include/c++/12/aarch64-unknown-linux-gnu
    ```

3. 验证LLVM/Clang是否安装成功。

    ```shell
    llvm-config --version        # 15.0.4
    clang --version | head -1
    ldconfig -p | grep libLLVM-15
    ```

## 安装internet_commercial分支额外依赖<a name="install_extra_dependencies"></a>

`vectorization`与`reader`模块引入了以下依赖，公共依赖中不包含，需要额外安装。

### 安装系统软件包<a name="install_extra_system_packages"></a>

```shell
dnf install -y fmt-devel double-conversion-devel gflags-devel glog-devel \
  libevent-devel libunwind-devel libsodium-devel boost-devel lld
```

>![须知](public_sys-resources/icon-notice.gif) **须知：**
>`lld`是`build.sh`的必需项，`build.sh`固定使用`-fuse-ld=lld`链接。

### 安装folly<a name="install_folly"></a>

仓库未提供`folly-devel`软件包，需要从源码编译安装。

1. 编译folly。

    ```shell
    cd /opt/omni/src
    rm -rf folly && mkdir folly
    tar -xzf /opt/omni/downloads/folly-v2024.01.01.00.tar.gz -C folly --strip-components=1

    # 构建目录不能命名为build：
    # folly源码自带的build/fbcode_builder/CMake/FBBuildOptions.cmake是构建依赖
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

2. 安装folly。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >示例程序`logging_example`可能因snappy符号链接问题编译失败，`libfolly.so`生成成功后可直接执行安装。

    ```shell
    cmake --install .
    ldconfig
    ```

3. 验证folly是否安装成功：`/usr/local/lib/libfolly.so`文件与`/usr/local/lib/cmake/folly`目录存在即为正常。

### 安装ORC 1.7.4<a name="install_orc"></a>

`reader`模块强依赖orc，需要先完成ORC安装。ORC源码已在[获取ORC源码](#obtain_orc)章节克隆至`/opt/omni/src/orc`，其顶层目录即为源码根目录。

1. 确认ORC源码目录。

    ```shell
    ls /opt/omni/src/orc        # CMakeLists.txt  c++  cmake_modules  proto  tools ...
    ```

2. 设置ORC编译所需的环境变量。

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`*_HOME`环境变量必须全部设置，否则ORC会通过ExternalProject重新下载并编译lz4、zstd、zlib、snappy，其中snappy会编译失败。

    ```shell
    export PROTOBUF_HOME=/opt/Gluten
    export ZSTD_HOME=/usr/local
    export LZ4_HOME=/usr/local
    export ZLIB_HOME=/usr/local
    export ZLIB_NG_HOME=/usr/local
    export SNAPPY_HOME=/usr/local
    export PATH=/opt/Gluten/bin:$PATH
    ```

3. 编译并安装ORC。

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

### 生成liborc.so<a name="generate_liborc"></a>

ORC 1.7.4的`CMakeLists.txt`中固定为`add_library(orc STATIC ...)`，仅生成静态库`liborc.a`。`reader.so`链接`-lorc`时需要共享库，否则protobuf、zlib、lz4、zstd、snappy等依赖无法传递。

1. 由静态库生成共享库`liborc.so`。

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

2. 验证`liborc.so`的依赖是否全部解析成功。

    ```shell
    ldd /usr/local/lib/liborc.so.1.7.4 | grep 'not found'   # 无输出即为正常
    ```

## 编译OmniOperator<a name="build_omnioperator"></a>

### 获取源码并切换分支<a name="obtain_omnioperator_source"></a>

1. 克隆OmniOperator源码并切换到internet_commercial分支。

    ```shell
    cd /opt/omni/src
    rm -rf OmniOperator
    git clone https://gitcode.com/openeuler/OmniOperator.git
    cd OmniOperator

    git fetch https://gitcode.com/openeuler/OmniOperator.git \
      internet_commercial:refs/remotes/openeuler/internet_commercial
    git checkout -B internet_commercial refs/remotes/openeuler/internet_commercial
    ```

2. 创建`clang++-15`软链接。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >顶层`CMakeLists.txt`使用`clang++-15`编译，openEuler默认未提供该命令，需要创建软链接。

    ```shell
    ln -sf /usr/lib/llvm-15/bin/clang++ /usr/local/bin/clang++-15
    ```

### 编译native库<a name="build_native"></a>

1. 设置编译所需的环境变量。

    ```shell
    source /opt/omni/env.sh

    cd /opt/omni/src/OmniOperator
    export OMNI_HOME=/opt/omni/install_internet
    export OMNI_COMPILER_THREAD_COUNT=32

    # compute模块引用了Protobuf的io头文件但未传递include路径；
    # ORC头文件内部使用相对include，需要把orc目录本身加入搜索路径
    export CXXFLAGS="-I/opt/Gluten/include -I/opt/Adaptor/include \
      -I/usr/local/include -I/usr/local/include/orc"

    # libhdfs只存在于/opt/Adaptor/lib
    export LIBRARY_PATH="/opt/Adaptor/lib:/opt/Gluten/lib:/usr/local/lib64:/usr/local/lib"
    ```

2. 执行编译。

    ```shell
    bash build_scripts/build.sh release:java --exclude-test
    ```

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >- `OMNI_COMPILER_THREAD_COUNT`不建议设置为256。openEuler GCC 12在高并发编译SVE代码时可能触发`internal compiler error: Bus error ... during RTL pass: cprop`，设置为32时编译稳定。
    >- `build.sh`每次执行都会删除`build/CMakeCache.txt`，上述环境变量在每次编译时均会重新生效。
    >- `build.sh release:java`仅编译native部分，不生成JAR包，JAR包生成方法请参见[生成JAR包](#build_jar)。

### 生成JAR包<a name="build_jar"></a>

1. 编译java绑定。

    ```shell
    cd /opt/omni/src/OmniOperator/bindings/java
    mvn -B clean install -Domni.home="$OMNI_HOME" -DskipTests
    ```

2. 编译UDF。

    ```shell
    cd /opt/omni/src/OmniOperator/core/src/udf/java
    mvn -B clean install -DskipTests
    ```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>Maven需要访问远程仓库，华为云镜像已在[安装Maven](#install_maven)章节配置。纯离线环境需提前预热`~/.m2/repository`目录或整体拷贝该目录。

## 验证编译结果<a name="verify_build"></a>

### 检查产物<a name="check_artifacts"></a>

1. 查看库文件与java绑定产物。

    ```shell
    ls -lh /opt/omni/install_internet/lib
    ls -lh /opt/omni/install_internet/java-binding
    ```

    产物目录结构如下所示。

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

2. 查看JAR包产物。

    ```text
    bindings/java/target/boostkit-omniop-bindings-2.2.0-aarch64.jar
    core/src/udf/java/target/boostkit-omniop-udf-2.2.0-aarch64.jar
    ```

### 检查动态库依赖<a name="check_dynamic_dependencies"></a>

1. 检查`reader`模块的动态库依赖。

    ```shell
    ldd /opt/omni/install_internet/lib/libboostkit-omniop-reader-2.2.0-aarch64.so | grep 'not found'
    ```

    无输出即为正常。

2. 确认关键依赖已正确解析。

    ```output
    libarrow_dataset.so.1100 -> /usr/local/lib64/libarrow_dataset.so.1100
    libparquet.so.1100       -> /usr/local/lib64/libparquet.so.1100
    liborc.so                -> /usr/local/lib/liborc.so
    libhdfs.so.0.0.0         -> /opt/Adaptor/lib/libhdfs.so.0.0.0
    libjvm.so                -> /usr/lib/jvm/java-1.8.0-openjdk/jre/lib/aarch64/server/libjvm.so
    libprotobuf.so.32        -> /opt/Gluten/lib/libprotobuf.so.32
    ```

### 配置运行时库路径<a name="configure_runtime_path"></a>

1. 将运行时依赖的库目录写入动态库配置文件。

    ```shell
    cat > /etc/ld.so.conf.d/omni-adaptor.conf <<'EOF'
    /opt/Adaptor/lib
    /usr/lib/jvm/java-1.8.0-openjdk/jre/lib/aarch64/server
    EOF
    ldconfig
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >`/opt/Gluten/lib`已由[生成liborc.so](#generate_liborc)写入`/etc/ld.so.conf.d/gluten.conf`；`/usr/local/lib`、`/usr/local/lib64`与`/usr/lib/llvm-15/lib`分别由[配置动态库搜索路径](#configure_ld_so)与[安装LLVM/Clang](#install_llvm)配置。

2. 设置运行时环境变量。

    ```shell
    export LD_LIBRARY_PATH=/opt/omni/install_internet/lib:/opt/omni/install_internet/java-binding:$LD_LIBRARY_PATH
    ```

## 编译Gluten Omni后端<a name="build_gluten"></a>

本章介绍基于OmniOperator编译产物，从源码编译Gluten项目internet_commercial分支Omni后端（backends-omni）的完整操作步骤。编译产物为C++执行插件`libspark_columnar_plugin.so`与Spark插件包`gluten-omni-bundle-*.jar`。

Gluten的C++插件直接链接[验证编译结果](#verify_build)章节中`/opt/omni/install_internet/lib`下的`boostkit-omniop-*`动态库，Java侧依赖同一次编译安装到本地Maven仓库的`boostkit-omniop-bindings`，因此必须先完成OmniOperator的编译与部署。

本文所有命令默认在root用户下执行，Gluten源码目录统一为`/opt/omni/src/Gluten`。

>![须知](public_sys-resources/icon-notice.gif) **须知：**
>本章只编译Omni后端，全程不涉及Velox后端。请勿执行`dev/builddeps-veloxbe.sh`、`dev/buildbundle-veloxbe.sh`等任何带velox字样的脚本。

### 安装Gluten编译依赖<a name="gluten_install_toolchain"></a>

1. 安装sudo。

    ```shell
    dnf install -y sudo
    ```

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`cpp-omni/build.sh`内部执行的是`sudo make install`，裸机上默认没有sudo命令，缺少时会直接报错`sudo: command not found`。

2. 验证编译工具链。

    ```shell
    gcc --version | head -1        # 12.3.1
    cmake --version | head -1      # 3.27.9
    ninja --version                # 1.11.1
    java -version                  # 1.8.0_462
    mvn -v | head -1               # Apache Maven 3.8.3
    ```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- 基础编译工具链已在[安装编译工具链](#install_toolchain)章节安装，fmt、folly等依赖已在[安装internet_commercial分支额外依赖](#install_extra_dependencies)章节安装，无需重复安装。
>- Maven华为云中央仓镜像已在[安装Maven](#install_maven)章节配置，编译Gluten时直接复用。

### 配置Gluten运行时库路径<a name="gluten_configure_ld_so"></a>

C++插件运行期依赖OmniOperator与Protobuf的动态库，需要提前写入动态库配置。

```shell
printf '/opt/omni/install_internet/lib\n' > /etc/ld.so.conf.d/gluten-omni.conf
ldconfig
```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>`/opt/Gluten/lib`（Protobuf 3.21.9）已由[生成liborc.so](#generate_liborc)写入`/etc/ld.so.conf.d/gluten.conf`，`/usr/local/lib`与`/usr/local/lib64`已由[配置动态库搜索路径](#configure_ld_so)配置，无需重复添加。

### 获取Gluten源码<a name="gluten_obtain_source"></a>

1. 克隆internet_commercial分支。

    ```shell
    cd /opt/omni/src
    rm -rf Gluten
    git clone --depth 1 -b internet_commercial https://gitcode.com/openeuler/Gluten.git
    cd Gluten
    git log --oneline -1        # 290c9b6 !363 feat: introduce Celeborn for Shuffle
    ```

2. 确认Omni后端目录存在。

    ```shell
    ls cpp-omni backends-omni
    ```

    其中`cpp-omni`为C++执行插件源码与构建脚本，`backends-omni`为Java/Scala侧Omni后端模块。

### 准备Gluten编译环境变量<a name="gluten_prepare_env"></a>

创建环境变量脚本`/opt/omni/gluten_env.sh`。

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

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- `build.sh`依据`OMNI_HOME`拼接`$OMNI_HOME/lib/include`作为OmniOperator头文件路径，因此`OMNI_HOME`必须指向实际安装目录`/opt/omni/install_internet`，不能沿用`env.sh`中的默认值。
>- ORC头文件`orc/io/InputStream.hh`以`#include "Adaptor.hh"`方式引用同级的`orc/Adaptor.hh`，缺少`-I/usr/local/include/orc`时会报`fatal error: Adaptor.hh: No such file or directory`。

### 执行C++插件编译<a name="gluten_run_cpp_build"></a>

```shell
cd /opt/omni/src/Gluten
bash ./cpp-omni/build.sh
```

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- `build.sh`不带参数时按Release方式编译，并在`FMT_HOME`、`FOLLY_HOME`未设置的情况下自动从`gitee.com`克隆源码编译fmt 10.1.1与folly v2024.07.01.00，分别安装到`/usr/local`。
>- fmt与folly已经安装过时，可在执行前`export FMT_HOME=/usr/local FOLLY_HOME=/usr/local`跳过重复编译。
>- 编译成功时最后输出`[100%] Built target spark_columnar_plugin`。

### 验证C++插件<a name="gluten_verify_cpp"></a>

```shell
ls -lh cpp-omni/build/releases/libspark_columnar_plugin.so
ldd cpp-omni/build/releases/libspark_columnar_plugin.so | grep 'not found'   # 无输出即为正常
```

正常产物大小为5MB左右，关键依赖解析结果如下。

```output
libprotobuf.so.32                          -> /opt/Gluten/lib/libprotobuf.so.32
libre2.so.11                               -> /usr/local/lib64/libre2.so.11
libfmt.so.10                               -> /usr/local/lib64/libfmt.so.10
libboostkit-omniop-vector-2.2.0-aarch64.so -> /opt/omni/install_internet/lib/...
libboostkit-omniop-operator-2.2.0-aarch64.so -> /opt/omni/install_internet/lib/...
```

### 编译Arrow Java定制包<a name="gluten_build_arrow_java"></a>

`gluten-arrow`模块依赖`org.apache.arrow:arrow-*:15.0.0-gluten`，该定制版本未发布到任何公共Maven仓库，需要基于Apache Arrow 15.0.0源码，叠加Gluten仓库自带的补丁后自行安装到本地仓库。

>![须知](public_sys-resources/icon-notice.gif) **须知：**
>本节只编译Arrow的Java部分，不编译Arrow C++，与Velox无关。缺少该步骤时，`mvn`阶段会报`Could not resolve dependencies ... org.apache.arrow:arrow-vector:jar:15.0.0-gluten`。

1. 下载并解压Arrow 15.0.0源码。

    ```shell
    cd /opt/omni/downloads
    curl -fsSL -o apache-arrow-15.0.0.tar.gz \
      https://mirrors.huaweicloud.com/apache/arrow/arrow-15.0.0/apache-arrow-15.0.0.tar.gz

    rm -rf /opt/omni/src/arrow-15.0.0 && mkdir -p /opt/omni/src/arrow-15.0.0
    tar -xzf apache-arrow-15.0.0.tar.gz -C /opt/omni/src/arrow-15.0.0 --strip-components=1
    ```

2. 应用Gluten仓库中的Arrow补丁。

    ```shell
    cd /opt/omni/src/arrow-15.0.0
    patch -p1 < /opt/omni/src/Gluten/ep/build-velox/src/modify_arrow_dataset_scan_option.patch
    patch -p1 < /opt/omni/src/Gluten/ep/build-velox/src/modify_arrow.patch
    ```

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`modify_arrow_dataset_scan_option.patch`新增了`org.apache.arrow.dataset.scanner.FragmentScanOptions`等类，不应用该补丁时`gluten-arrow`编译会报`object FragmentScanOptions is not a member of package org.apache.arrow.dataset.scanner`。

3. 修改版本号并安装到本地Maven仓库。

    ```shell
    cd /opt/omni/src/arrow-15.0.0/java
    mvn -B versions:set -DnewVersion=15.0.0-gluten -DprocessAllModules -DgenerateBackupPoms=false

    mvn -B clean install -P arrow-c-data,arrow-jni \
      -pl bom,maven/module-info-compiler-maven-plugin,vector,c,dataset -am \
      -DskipTests -Drat.skip=true -Dmaven.gitcommitid.skip=true \
      -Dcheckstyle.skip=true -Dmaven.javadoc.skip=true
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >`c`、`dataset`模块位于`arrow-c-data`、`arrow-jni`两个profile中，必须显式激活；只需生成JAR包，因此不需要`-Parrow-jni`中的JNI本地库编译步骤。

4. 验证本地仓库中的构件。

    ```shell
    ls /root/.m2/repository/org/apache/arrow/*/15.0.0-gluten/*.jar
    unzip -l /root/.m2/repository/org/apache/arrow/arrow-dataset/15.0.0-gluten/arrow-dataset-15.0.0-gluten.jar \
      | grep FragmentScanOptions
    ```

    能够看到`arrow-vector`、`arrow-memory-core`、`arrow-memory-unsafe`、`arrow-c-data`、`arrow-dataset`等JAR，且`arrow-dataset`中包含`FragmentScanOptions.class`即为正常。

### 编译Java/Scala插件包<a name="gluten_build_java"></a>

1. 加载环境变量并执行打包。

    ```shell
    source /opt/omni/gluten_env.sh
    cd /opt/omni/src/Gluten

    mvn clean package -Pbackends-omni -Pspark-3.5 -DskipTests \
      -Dspotless.check.skip=true -Dscalastyle.skip=true -Dcheckstyle.skip=true
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >- `-Pbackends-omni`激活Omni后端模块（`gluten-arrow`、`backends-omni`），`-Pspark-3.5`指定Spark 3.5.2。
    >- 必须使用JDK 8，首次执行需要从华为云镜像下载全部依赖，耗时较长。
    >- 编译成功时最后输出`BUILD SUCCESS`。

2. 查看打包结果。

    ```shell
    ls -lh package/target/gluten-omni-bundle-*.jar
    ```

### 验证Gluten编译结果<a name="gluten_verify_build"></a>

1. 检查产物。

    ```text
    /opt/omni/src/Gluten/
    ├── cpp-omni/build/releases/libspark_columnar_plugin.so   5.0M
    ├── backends-omni/target/backends-omni-1.3.0-3.5.jar      1.5M
    └── package/target/
        ├── gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar   28M
        └── gluten-package-1.3.0.jar
    ```

    JAR包名中的`openEuler_24.03_aarch_64`由编译机操作系统与架构自动生成，与目标运行环境需一致。

2. 检查C++插件的动态库依赖。

    ```shell
    ldd /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so | grep 'not found'
    ```

    无输出即为正常。若提示`libfmt.so.10`或`libboostkit-omniop-*`找不到，说明`ldconfig`未生效，重新执行[配置Gluten运行时库路径](#gluten_configure_ld_so)后重试。

### Gluten编译注意事项<a name="gluten_build_notes"></a>

- `cpp-omni/build.sh`每次执行都会删除`cpp-omni/build`目录并重新执行CMake配置，上述环境变量在每次编译时均需重新生效。
- `backends-omni/pom.xml`中资源目录引用了`${cpp.releases.dir}`，该属性仅在`backends-velox`中定义，因此`libspark_columnar_plugin.so`不会被打进`gluten-omni-bundle`包，需要与OmniOperator的`lib`目录一起独立部署。
- `cpp-omni`以`-lboostkit-omniop-{vector,operator,codegen,reader}`链接OmniOperator，OmniOperator重新编译后需要重新编译C++插件。
- 编译全程不得执行`dev/`目录下与velox相关的脚本；本节的依赖准备（fmt、folly、Arrow Java）均由`cpp-omni/build.sh`与Maven自行完成。

## 制作发布包<a name="package_release"></a>

OmniOperator与Gluten编译完成后，可将[验证编译结果](#verify_build)与[验证Gluten编译结果](#gluten_verify_build)中的产物打包为[安装指南](installation_guide.md)表3所需的发布包，使编译产物可直接用于安装部署。

打包环境与编译环境保持一致：openEuler 24.03 LTS aarch64，OmniOperator internet_commercial分支2.2.0，Gluten internet_commercial分支1.3.0，适配Spark 3.5.2。本章所有命令默认在root用户下执行，打包目录统一为`/opt/omni/release`。

**表 2** 发布包清单<a id="package_list"></a>

|压缩包|对应安装指南序号|主要内容|
|--|--|--|
|BoostKit-omniop_2.2.0.zip|表3序号1（OmniRuntime压缩包的内层包）|OmniOperator算子库、头文件、Java绑定JAR包、UDF JAR包、libboundscheck.so|
|BoostKit-omniruntime-gluten-2.2.0.zip|表3序号4（Gluten）|OmniOperator算子库、头文件、Java绑定、Gluten C++插件、Gluten插件JAR包、插件运行所依赖的第三方库|
|Dependency_library_Gluten.zip|表3序号4（Gluten）|libLLVM-15.so、libjemalloc.so.2|
|Dependency_library_openeuler24.03.zip|表3序号5（Dependency_library类别，本分支新增的openEuler 24.03包，安装指南无同名条目）|libLLVM-15.so、libjemalloc.so.2|

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- `BoostKit-omniruntime_1.9.0.zip`是OmniRuntime产品的外层压缩包，除OmniOperator安装包外还包含闭源组件`BoostKit-omniadvisor_core_2.0.0.zip`、许可协议与版本文件，无法由源码编译产出。本章只制作其中可从编译产物完整复现的内层`BoostKit-omniop_2.2.0.zip`。
>- LLVM与jemalloc需要在目标操作系统上编译才能正常运行，因此不放入OmniOperator安装包，单独作为Dependency_library包发布。
>- 包内二进制与编译环境绑定，openEuler 24.03的产物不能直接用于CentOS或openEuler 20.03/22.03。

### 打包前检查<a name="package_preparation"></a>

打包输入来自[编译OmniOperator](#build_omnioperator)与[编译Gluten Omni后端](#build_gluten)两个阶段，执行前请确认[**表 3**](#package_inputs)中的文件均已生成。

**表 3** 打包输入清单<a id="package_inputs"></a>

|类别|来源路径|说明|
|--|--|--|
|OmniOperator算子库|`/opt/omni/install_internet/lib/libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|Gluten插件链接的四类算子库|
|OmniOperator头文件|`/opt/omni/install_internet/lib/include`|供二次开发与部署配套使用|
|Java绑定库|`/opt/omni/install_internet/java-binding/libboostkit-omniop-java-binding-2.2.0-aarch64.so`|Java侧调用OmniOperator的JNI库|
|Java绑定JAR包|`/opt/omni/src/OmniOperator/bindings/java/target/boostkit-omniop-bindings-2.2.0-aarch64.jar`|内含上述JNI库的Java调用包|
|UDF JAR包|`/opt/omni/src/OmniOperator/core/src/udf/java/target/boostkit-omniop-udf-2.2.0-aarch64.jar`|UDF插件使用的JAR包|
|securec实现|`/usr/local/lib/libboundscheck.so`|算子库以`-lboundscheck`链接的securec开源实现|
|Gluten C++插件|`/opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so`|`cpp-omni/build.sh`产物|
|Gluten插件JAR包|`/opt/omni/src/Gluten/package/target/gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar`|`mvn package`产物|
|第三方运行库|Protobuf、Arrow、ORC、HDFS、fmt、re2、folly、boundscheck、ABSL等|编译各依赖时安装到`/opt/Gluten/lib`、`/usr/local/lib`、`/usr/local/lib64`|
|LLVM与jemalloc|`/usr/lib/llvm-15/lib/libLLVM-15.so`、`/usr/local/lib/libjemalloc.so.2`|单独打包为Dependency_library包|

1. 检查上表文件是否存在。

    ```shell
    ls -lh /opt/omni/install_internet/lib /opt/omni/install_internet/java-binding
    ls -lh /usr/local/lib/libboundscheck.so
    ls -lh /opt/omni/src/OmniOperator/bindings/java/target/*-aarch64.jar
    ls -lh /opt/omni/src/OmniOperator/core/src/udf/java/target/*-aarch64.jar
    ls -lh /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so
    ls -lh /opt/omni/src/Gluten/package/target/gluten-omni-bundle-*.jar
    ```

2. 创建打包目录。

    ```shell
    mkdir -p /opt/omni/release/stage
    ```

### 制作运行时依赖清单<a name="collect_runtime_dependencies"></a>

插件与算子库依赖大量第三方库，逐个手工列举容易遗漏。此处通过`ldd`递归解析依赖闭包，自动收集所有非系统目录的库文件，作为打包依据。

1. 创建依赖收集脚本`/opt/omni/collect_runtime_deps.sh`。

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

2. 执行脚本并查看结果。

    ```shell
    bash /opt/omni/collect_runtime_deps.sh
    ```

    本次编译环境共收集到81个库文件，覆盖Protobuf、Arrow、ORC、HDFS、fmt、re2、folly、boundscheck、ABSL与OmniOperator自身库。

### 制作BoostKit-omniop_2.2.0.zip<a name="make_omniop_package"></a>

该包对应[安装指南](installation_guide.md)表3序号1，是`BoostKit-omniruntime_1.9.0.zip`解压后得到的内层安装包，内容与[**表 3**](#package_inputs)中的输入一一对应。

1. 组装包目录。

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

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >`boostkit-omniop-bindings-2.2.0-aarch64.jar`内已包含`libboostkit-omniop-java-binding-2.2.0-aarch64.so`，与官方包结构一致，无需再单独拷贝该so文件。

2. 生成tar.gz。

    ```shell
    cd /opt/omni/release/stage
    tar --owner root --group root -zcf boostkit-omniop-operator-2.2.0-aarch64.tar.gz boostkit-omniop-operator-2.2.0-aarch64
    ```

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >tar.gz必须保留顶层目录`boostkit-omniop-operator-2.2.0-aarch64/`，安装指南中的`cd /opt/omni-operator/boostkit-omniop-operator-2.2.0-aarch64`依赖该目录层级。

3. 压缩为zip并计算校验值。

    ```shell
    cd /opt/omni/release/stage
    zip -q /opt/omni/release/BoostKit-omniop_2.2.0.zip boostkit-omniop-operator-2.2.0-aarch64.tar.gz
    sha256sum /opt/omni/release/BoostKit-omniop_2.2.0.zip
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >OmniOperator源码目录提供了出包脚本`build.sh package`，其内部流程与本节的组装步骤一致：拷贝`$OMNI_HOME/lib`、拷贝两个`*-aarch64.jar`、`tar`、`zip`。使用该脚本需要设置`OMNI_HOME`与`JAVA_HOME`，并在源码目录同级存在`libboundscheck`、`json`目录，脚本会重新编译整个工程。已有编译产物时，直接按本节步骤手工组装即可，避免重复编译。

### 制作BoostKit-omniruntime-gluten-2.2.0.zip<a name="make_omniruntime_gluten_package"></a>

该包对应[安装指南](installation_guide.md)表3序号4的Gluten场景，包含OmniOperator产物、Gluten产物以及插件运行所需的第三方库。

1. 创建包目录并拷贝OmniOperator头文件。

    ```shell
    PKG=/opt/omni/release/stage/BoostKit-omniruntime-gluten-2.2.0
    rm -rf "$PKG" && mkdir -p "$PKG"
    cd "$PKG"
    cp -a /opt/omni/install_internet/lib/include ./
    ```

2. 拷贝依赖闭包中的库文件。

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >`libLLVM-15.so`与`libjemalloc.so.2`不放入本包，由Dependency_library包提供，避免重复打包约70MB。

    ```shell
    while read -r f; do
      case "$f" in
        */libLLVM-15.so|*/libjemalloc.so.2) continue ;;
      esac
      cp -L "$f" .
    done < /tmp/gluten_closure.txt
    ```

3. 拷贝Gluten插件与JAR包。

    ```shell
    cp -L /opt/omni/install_internet/java-binding/libboostkit-omniop-java-binding-2.2.0-aarch64.so .
    cp -L /opt/omni/src/Gluten/cpp-omni/build/releases/libspark_columnar_plugin.so .
    cp -L /opt/omni/src/Gluten/package/target/gluten-omni-bundle-*.jar .
    ```

4. 生成README.txt说明文件。

    ```shell
    cat > README.txt <<'EOF'
    BoostKit OmniRuntime Gluten 2.2.0
    =================================
    适配环境：openEuler 24.03 LTS aarch64，Gluten internet_commercial分支，Spark 3.5.2

    目录说明：
    - include/                                        OmniOperator头文件
    - libboostkit-omniop-*.so                         OmniOperator算子库（vector/operator/codegen/reader）
    - libboostkit-omniop-java-binding-*.so            Java绑定动态库
    - libspark_columnar_plugin.so                     Gluten C++执行插件
    - gluten-omni-bundle-*.jar                        Gluten Spark插件包
    - libprotobuf/libarrow/liborc/libhdfs/libfmt等    插件运行所依赖的第三方库

    安装方式见《安装指南》Gluten场景章节：将上述文件拷贝到/opt/omni-operator/lib。
    LLVM-15与jemalloc由Dependency_library_Gluten.zip提供。
    EOF
    ```

5. 压缩发布包。

    ```shell
    cd /opt/omni/release/stage
    zip -r -q /opt/omni/release/BoostKit-omniruntime-gluten-2.2.0.zip BoostKit-omniruntime-gluten-2.2.0
    ```

    压缩包内保留顶层目录`BoostKit-omniruntime-gluten-2.2.0/`，解压后即为安装目录内容。

### 制作Dependency_library压缩包<a name="make_dependency_library_package"></a>

Dependency_library包只包含必须在目标操作系统上编译的LLVM与jemalloc，Gluten与OmniOperator场景的两个包内容一致，差异仅在命名。

1. 制作Gluten场景依赖包。

    ```shell
    cd /opt/omni/release/stage
    mkdir -p Dependency_library_Gluten
    cp -L /usr/lib/llvm-15/lib/libLLVM-15.so Dependency_library_Gluten/
    cp -L /usr/local/lib/libjemalloc.so.2 Dependency_library_Gluten/
    zip -r -q /opt/omni/release/Dependency_library_Gluten.zip Dependency_library_Gluten
    ```

2. 制作按操作系统命名的依赖包。

    ```shell
    cd /opt/omni/release/stage
    mkdir -p Dependency_library_openeuler24.03
    cp -L /usr/lib/llvm-15/lib/libLLVM-15.so Dependency_library_openeuler24.03/
    cp -L /usr/local/lib/libjemalloc.so.2 Dependency_library_openeuler24.03/
    zip -r -q /opt/omni/release/Dependency_library_openeuler24.03.zip Dependency_library_openeuler24.03
    ```

3. 计算全部压缩包的校验值。

    ```shell
    cd /opt/omni/release
    sha256sum *.zip > SHA256SUM.txt
    cat SHA256SUM.txt
    ```

### 包内容明细<a name="package_contents"></a>

**表 4** BoostKit-omniop_2.2.0.zip内容<a id="omniop_package_contents"></a>

|分类|文件|数量|说明|
|--|--|--|--|
|头文件|`include/`（`vector`、`util`、`codegen`、`operator`等）|262|OmniOperator开发头文件|
|算子库|`libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|4|算子库本体|
|securec实现|`libboundscheck.so`|1|算子库以`-lboundscheck`方式链接|
|Java绑定|`boostkit-omniop-bindings-2.2.0-aarch64.jar`|1|内含`libboostkit-omniop-java-binding-2.2.0-aarch64.so`|
|UDF插件|`boostkit-omniop-udf-2.2.0-aarch64.jar`|1|UDF场景使用|

打包结果：压缩包约19MB，tar.gz内共300个条目（含目录），与官方`BoostKit-omniop_2.0.0.zip`的目录结构一致。

**表 5** BoostKit-omniruntime-gluten-2.2.0.zip内容<a id="boostkit_package_contents"></a>

|分类|文件|数量|说明|
|--|--|--|--|
|头文件|`include/`（`vector`、`util`、`codegen`、`operator`等）|262|OmniOperator开发头文件|
|OmniOperator算子库|`libboostkit-omniop-{vector,operator,codegen,reader}-2.2.0-aarch64.so`|4|插件编译期链接、运行期加载|
|Java绑定库|`libboostkit-omniop-java-binding-2.2.0-aarch64.so`|1|Java侧JNI调用|
|Gluten插件|`libspark_columnar_plugin.so`|1|Gluten执行计划下推入口|
|Gluten插件包|`gluten-omni-bundle-spark3.5_2.12-openEuler_24.03_aarch_64-1.3.0-2.2.0.jar`|1|Spark插件包|
|第三方运行库|`libprotobuf.so.32`、`libarrow.so.1100`、`libarrow_dataset.so.1100`、`libparquet.so.1100`、`liborc.so`、`libhdfs.so.0.0.0`、`libfmt.so.10`、`libre2.so.11`、`libfolly.so.0.58.0-dev`、`libboundscheck.so`|10|编译期安装到`/opt/Gluten/lib`、`/usr/local/lib`、`/usr/local/lib64`|
|ABSL库|`libabsl_*.so.2407.0.0`|64|ABSL 20240722.0，Gluten运行依赖|
|说明文件|`README.txt`|1|版本与目录说明|

打包结果：压缩包约105MB，解压后约292MB，共375个文件。

**表 6** Dependency_library压缩包内容<a id="dependency_package_contents"></a>

|文件|大小|说明|
|--|--|--|
|`libLLVM-15.so`|67MB|`codegen`模块运行依赖|
|`libjemalloc.so.2`|5.5MB|内存分配器，需在目标OS编译|

### 打包后校验<a name="package_verification"></a>

1. 检查压缩包结构。

    ```shell
    cd /opt/omni/release
    unzip -l BoostKit-omniop_2.2.0.zip
    unzip -l BoostKit-omniruntime-gluten-2.2.0.zip | head -20
    unzip -l BoostKit-omniruntime-gluten-2.2.0.zip | tail -2
    unzip -l Dependency_library_Gluten.zip
    ```

2. 解压后检查依赖是否可全部解析。

    ```shell
    rm -rf /tmp/ziptest && mkdir -p /tmp/ziptest && cd /tmp/ziptest
    unzip -q /opt/omni/release/BoostKit-omniruntime-gluten-2.2.0.zip
    unzip -q /opt/omni/release/Dependency_library_Gluten.zip

    P=/tmp/ziptest/BoostKit-omniruntime-gluten-2.2.0
    cp Dependency_library_Gluten/libLLVM-15.so Dependency_library_Gluten/libjemalloc.so.2 "$P"/

    LD_LIBRARY_PATH="$P" ldd "$P/libspark_columnar_plugin.so" | grep 'not found'
    LD_LIBRARY_PATH="$P" ldd "$P/libboostkit-omniop-reader-2.2.0-aarch64.so" | grep 'not found'
    ```

    两条命令均无输出即为正常，说明插件与算子库的依赖已全部包含在包内。

3. 校验Gluten插件的ABSL依赖版本。

    ```shell
    LD_LIBRARY_PATH="$P" ldd "$P/libspark_columnar_plugin.so" | grep 'libabsl'
    ```

    输出的库名均应为`libabsl_*.so.2407.0.0`，且解析路径均应指向`$P`中的库文件。若出现其他soname版本或`not found`，说明包内ABSL版本与插件不匹配，不能直接跳过[安装指南](installation_guide.md)中的ABSL编译安装步骤。

### 产物使用说明<a name="package_to_installation"></a>

打包产物可直接用于[安装指南](installation_guide.md)的安装流程：Gluten场景使用`BoostKit-omniruntime-gluten-2.2.0.zip`与`Dependency_library_Gluten.zip`；OmniOperator核心算子库与运行依赖分别由`BoostKit-omniop_2.2.0.zip`和对应OS的`Dependency_library`包提供。

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- Gluten包内已内置64个ABSL库（20240722.0）。仅当[打包后校验](#package_verification)中插件的ABSL依赖均解析为包内的`libabsl_*.so.2407.0.0`时，才可跳过[安装指南](installation_guide.md)Gluten场景中手动编译ABSL的步骤；否则应按安装指南编译安装插件所需的ABSL版本。
>- 安装指南的依赖安装命令以openEuler 22.03为例。在openEuler 24.03环境使用本文生成的依赖包时，请将命令中的`Dependency_library_openeuler22.03.zip`及解压目录`Dependency_library_openeuler22.03`分别替换为`Dependency_library_openeuler24.03.zip`和`Dependency_library_openeuler24.03`，否则将因文件或目录不存在而执行失败。
>- `BoostKit-omniop_2.2.0.zip`只包含OmniOperator自身产物，不含folly、protobuf、orc、arrow等第三方库。官方2.0.0将这些依赖静态链接进算子库，本分支为动态链接，部署到目标机时需一并提供这些库。
>- 本分支使用`libboundscheck.so`替代官方包中的`libsecurec.so`，按安装指南执行拷贝命令时请以包内实际文件名为准。

## 部署说明<a name="deployment_notes"></a>

- `/opt/omni/install_internet/lib`为部署使用的库目录。`libboundscheck.so`、`liborc.so`、`libfolly.so`、`libarrow*`属于外部依赖，不在该目录内，目标机需一并安装或加入`LD_LIBRARY_PATH`。
- 目标机如果没有`/opt/Adaptor`目录，需将`libhdfs.so*`、`libhdfspp.so*`与`include/`一并拷贝至目标机。
- Gluten Omni后端的`libspark_columnar_plugin.so`与`gluten-omni-bundle-*.jar`位于`/opt/omni/src/Gluten`下，不属于`/opt/omni/install_internet`目录，需与OmniOperator的`lib`目录一并部署。
- 版本信息以[验证编译结果](#verify_build)章节的验证输出为准。
