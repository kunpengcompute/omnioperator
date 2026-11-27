# 快速入门<a name="quick_start"></a>

本文档介绍OmniOperator算子加速的快速安装与配置方法，覆盖SparkExtension和Gluten两种启用方式，并以TPC-DS Q82为例验证查询性能提升效果。

## OmniOperator算子加速介绍<a name="omni_operator_introduction"></a>

OmniOperator算子加速是鲲鹏BoostKit大数据OmniRuntime的特性之一，使用Native Code（C/C++）实现大数据SQL算子，通过OmniVec列式内存组织（零拷贝、无序列化开销）和向量化执行技术，利用鲲鹏向量化指令替换Spark、Hive等引擎中的Java算子，提升SQL查询性能，适用于大规模数据融合分析场景，已适配Spark 3.1.1/3.3.1/3.4.3/3.5.2、Hive 3.1.0和Gluten 1.3。本文以TPC-DS测试集Q82为例，介绍通过SparkExtension和Gluten两种方式启用OmniOperator算子加速的快速安装、配置与结果验证方法，指导用户快速上手并直观感受查询性能提升。

## 快速安装OmniOperator算子加速<a name="quick_install"></a>

本文主要介绍如何快速完成OmniOperator算子加速的安装。如需了解组网规划、多节点部署等详细操作，请参见《[安装指南](installation_guide.md)》。

### 安装前准备<a name="installation_preparation"></a>

本节所有安装命令需root权限，请使用`sudo`或切换到root用户执行。建议采用存算一体组网：1台管理节点（server1）用于提交任务，3台计算节点（agent1～agent3）用于运行OmniOperator。单机部署时，以下操作均在同一台节点执行。

执行以下命令检查环境，所有命令执行成功且符合预期后，再继续安装。

**表 1** 环境检查<a id="environment_check"></a>

|检查项|执行命令|预期结果|失败处理|
|--|--|--|--|
|操作系统检查|cat /etc/os-release|Gluten场景为openEuler 22.03 LTS SP4（其他22.03补丁版本亦可）或24.03 LTS SP1；SparkExtension场景为CentOS 7.9、openEuler 20.03 LTS SP1或22.03 LTS SP4（其他22.03补丁版本亦可）|更换为受支持的操作系统版本|
|SVE指令集检查（Gluten场景）|cat /proc/cpuinfo \| grep sve \| head -n 1|有输出，表示CPU支持SVE指令集|更换支持SVE的鲲鹏服务器，或改用SparkExtension方式启用|
|JDK检查|java -version|毕昇JDK 1.8.0_342（openEuler 22.03场景）；JDK 17（openEuler 24.03场景）|安装对应版本JDK，并正确配置JAVA_HOME|
|Hadoop检查|hdfs dfs -ls /|正常列出HDFS根目录内容|启动HDFS服务，或检查core-site.xml中的fs.defaultFS配置|
|Spark检查|spark-sql --version|Gluten场景为3.3.1、3.4.3或3.5.2；SparkExtension场景为3.1.1、3.3.1、3.4.3或3.5.2|安装对应版本Spark，并配置SPARK_HOME|
|内存检查|free -h|可用内存不小于32GB（推荐384GB）|关闭占用内存的业务进程，或扩容内存|
|磁盘检查|df -h /opt|/opt可用空间不小于50GB|清理磁盘，或更换安装目录|

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- 硬件要求：鲲鹏920系列处理器或鲲鹏950处理器，推荐384GB内存（12×32GB）。
>- 本文以openEuler 22.03 LTS SP4为实操环境，以Spark 3.3.1/3.5.2、OmniOperator 2.2.0为例进行说明；openEuler 22.03的其他补丁版本（如SP1、SP2、SP3）操作步骤相同，其他受支持版本仅软件包名称中的版本号不同。

### 准备环境依赖<a name="prepare_environment"></a>

1. 安装基础工具。

    ```shell
    yum install -y tar unzip gzip xz curl wget which findutils procps-ng
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >openEuler基础镜像默认不包含`unzip`，如未安装，后续解压安装包时会提示`unzip: command not found`。

2. 配置动态库搜索路径。

    ```shell
    cat > /etc/ld.so.conf.d/usr-local.conf <<'EOF'
    /usr/local/lib
    /usr/local/lib64
    EOF
    ldconfig
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >本文使用预编译安装包；SparkExtension场景无需安装GCC、CMake、Maven等编译工具链。Gluten场景在编译ABSL时需要GCC/G++、CMake和Make。如需从源码编译OmniOperator，完整的系统准备和依赖安装步骤请参见《源码编译指南》。

### 获取软件包<a name="obtain_packages"></a>

从鲲鹏社区或代码托管平台获取安装包，并按场景选择。本文使用的软件包如[**表 2**](#software_packages)所示。

**表 2** 软件包获取列表<a id="software_packages"></a>

|软件类型|必选/可选|软件包说明|软件包名称|获取链接|
|--|--|--|--|--|
|OmniRuntime压缩包|SparkExtension场景必选|解压后获得OmniOperator算子加速安装包|BoostKit-omniruntime_1.9.0.zip|[OmniRuntime压缩包下载](https://kunpeng-repo.obs.cn-north-4.myhuaweicloud.com/Kunpeng%20BoostKit/Kunpeng%20BoostKit%2025.3.0/BoostKit-omniruntime_1.9.0.zip)|
|SparkExtension|SparkExtension场景必选|Spark引擎扩展包，按Spark版本选择|boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip|[SparkExtension下载](https://gitcode.com/boostkit/boostkit-bigdata/releases/download/Kunpeng-BoostKit-25.3.0-OmniOperator-release/boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip)|
|Dependency_library|SparkExtension场景必选|运行时依赖库，按操作系统选择|Dependency_library_openeuler22.03.zip|[Dependency_library下载](https://gitcode.com/boostkit/boostkit-bigdata/releases/download/Kunpeng-BoostKit-25.3.0-OmniOperator-release/Dependency_library_openeuler22.03.zip)|
|Gluten|Gluten场景必选|Gluten适配包，按Spark版本选择：Spark 3.5.2对应2.2.0，Spark 3.4.3对应2.0.0，Spark 3.3.1对应1.0.0|BoostKit-omniruntime-gluten-2.2.0.zip|[Gluten下载](https://atomgit.com/openeuler/OmniOperator/releases/download/26.1.0-OmniOperator-2.2.0-release/BoostKit-omniruntime-gluten-2.2.0.zip)|
|Dependency_library_Gluten|Gluten场景必选|Gluten运行时依赖库|Dependency_library_Gluten.zip|[Dependency_library_Gluten下载](https://atomgit.com/openeuler/OmniOperator/releases/download/26.1.0-OmniOperator-2.2.0-release/Dependency_library_Gluten.zip)|

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- 使用软件包前，请先阅读《[鲲鹏应用使能套件BoostKit用户许可协议 2.0](https://www.hikunpeng.com/zh/legal/developer/boostkit/software/protocol)》。
>- SparkExtension场景还需安装与Spark版本匹配的扩展包；Gluten场景仅支持Spark 3.3.1、3.4.3和3.5.2。

### 安装运行依赖<a name="install_dependencies"></a>

1. 安装操作系统依赖。

    ```shell
    yum install lz4-devel zstd-devel snappy-devel protobuf-c-devel protobuf-lite-devel \
      boost-devel cyrus-sasl-devel jsoncpp-devel openssl-devel libatomic -y

    # Gluten场景需额外安装以下依赖
    yum install double-conversion* gflags* glog* libaio numactl-libs -y
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >若`yum`源不可用，可先配置本地Yum源。以openEuler 22.03 LTS SP4为例，执行以下命令：
    >
    >```shell
    >dnf config-manager --add-repo https://repo.oepkgs.net/openeuler/rpm/openEuler-22.03-LTS-SP4/extras/aarch64/
    >```

2. 部署LLVM和jemalloc。

    ```shell
    mkdir -p /opt/omni-operator/lib
    cd /opt/omni-operator

    # SparkExtension场景改为 DEP_DIR=Dependency_library_openeuler22.03
    DEP_DIR=Dependency_library_Gluten
    unzip ${DEP_DIR}.zip
    cp ${DEP_DIR}/* /opt/omni-operator/lib/
    ```

    >![须知](public_sys-resources/icon-notice.gif) **须知：**
    >- 必须拷贝`Dependency_library`目录下的全部库文件（包括Arrow、Parquet、ORC等）。若只拷贝`libLLVM-15.so`和`libjemalloc.so.2`，执行ORC读写时会提示`libarrow_dataset.so.1100: cannot open shared object file`。
    >- 如果环境中已安装过LLVM和jemalloc，建议先删除旧的`libLLVM-15.so`和`libjemalloc.so.2`，再执行拷贝命令。

3. （Gluten场景）编译并安装ABSL库。

    ```shell
    git clone https://github.com/abseil/abseil-cpp.git
    cd abseil-cpp
    git checkout tags/20250127.0

    mkdir build && cd build
    cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      -DABSL_PROPAGATE_CXX_STD=ON -DBUILD_SHARED_LIBS=ON
    make -j32
    make install

    cp /usr/local/lib64/libabsl_* /opt/omni-operator/lib/
    ```

4. 配置环境变量。

    ```shell
    echo "export OMNI_HOME=/opt/omni-operator" >> ~/.bashrc
    echo 'export LD_LIBRARY_PATH=$OMNI_HOME/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
    source ~/.bashrc
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >若环境中原本不存在`LD_LIBRARY_PATH`，配置时应直接设置为`export LD_LIBRARY_PATH=$OMNI_HOME/lib`，避免将当前目录引入动态库查找路径。

### 安装OmniOperator算子加速软件包<a name="install_omnioperator"></a>

请根据启用方式选择以下一种安装方式。

>![说明](public_sys-resources/icon-note.gif) **说明：**
>
>- 若提示`Permission denied`，请确认当前用户为root；若提示依赖缺失，请返回[安装运行依赖](#install_dependencies)完成依赖安装。
>- 若提示`No such file or directory`，请确认安装包已上传至对应目录，且包名中的版本号与实际文件一致。

#### 方式一：安装SparkExtension场景软件包<a name="install_sparkextension_package"></a>

1. 将`BoostKit-omniruntime_1.9.0.zip`、`BoostKit-omniop_2.0.0.zip`和SparkExtension扩展包上传至`/opt/omni-operator`目录，并解压。

    ```shell
    cd /opt/omni-operator
    unzip BoostKit-omniruntime_1.9.0.zip
    unzip BoostKit-omniop_2.0.0.zip
    tar -zxvf boostkit-omniop-operator-2.0.0-aarch64-openeuler.tar.gz
    ```

2. 拷贝库文件和头文件，并设置权限。

    ```shell
    cd /opt/omni-operator/boostkit-omniop-operator-2.0.0-aarch64
    cp -r include libboostkit* boostkit-omniop* libsecurec.so /opt/omni-operator/lib/
    chmod -R 550 /opt/omni-operator/lib/*
    ```

3. 创建配置目录和配置文件。

    ```shell
    mkdir -p /opt/omni-operator/conf
    touch /opt/omni-operator/conf/omni.conf
    chmod 750 /opt/omni-operator/conf
    chmod 640 /opt/omni-operator/conf/omni.conf
    ```

4. 部署SparkExtension扩展包。

    ```shell
    cd /opt/omni-operator
    unzip boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip
    unzip boostkit-omniop-spark-3.3.1-2.0.0-aarch64-openeuler.zip
    mv boostkit-omniop-spark-3.3.1-2.0.0-aarch64.jar ./lib/
    tar -zxvf dependencies.tar.gz -C ./lib
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >`boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip`中包含NEON实现（`-openeuler.zip`）和SVE实现（`-openeuler-sve.zip`）两个包，请根据服务器是否支持SVE指令集选择。本文以NEON实现为例。

#### 方式二：安装Gluten场景软件包<a name="install_gluten_package"></a>

1. 将`BoostKit-omniruntime-gluten-2.2.0.zip`和`Dependency_library_Gluten.zip`上传至`/opt`目录。

2. 创建安装脚本`/opt/install_gluten.sh`，写入以下内容。

    ```shell
    #!/bin/bash
    set -e
    set -u
    set -o pipefail

    SRC_DIR=/opt
    cd "$SRC_DIR"

    OMNI_HOME=/opt/omni-operator
    OE_VERSION_ID=$(. /etc/os-release && echo "$VERSION_ID")

    gluten_zips=(BoostKit-omniruntime-gluten-*.zip)
    if [ ! -f "${gluten_zips[0]}" ]; then
        echo "not found omni pkg in $SRC_DIR"
        exit 1
    fi
    OMNI_VERSION=$(basename "${gluten_zips[0]}" .zip | sed 's/BoostKit-omniruntime-gluten-//')

    TMP_DIR="$SRC_DIR/tmp"
    rm -rf "$TMP_DIR"
    mkdir -p "$TMP_DIR"

    mkdir -p "$OMNI_HOME/lib" "$OMNI_HOME/conf"
    touch "$OMNI_HOME/conf/omni.conf"

    unzip -o "BoostKit-omniruntime-gluten-${OMNI_VERSION}.zip" -d "$TMP_DIR"
    unzip -o Dependency_library_Gluten.zip -d "$TMP_DIR"

    if [ "$OMNI_VERSION" = "2.2.0" ]; then
        case "$OE_VERSION_ID" in
            22.03)
                TARGET_SUBDIR="openEuler22.03_JDK8"
                ;;
            24.03)
                TARGET_SUBDIR="openEuler24.03_JDK17"
                ;;
            *)
                echo "unsupported os version: openEuler${OE_VERSION_ID}"
                exit 1
                ;;
        esac
        cd "$TMP_DIR/$TARGET_SUBDIR" || { echo "Failed to cd to $TMP_DIR/$TARGET_SUBDIR"; exit 1; }
    else
        cd "$TMP_DIR" || exit 1
        rm -rf openEuler24.03_JDK17
    fi

    if ls BoostKit-omniruntime-omnioperator-*.zip 1>/dev/null 2>&1; then
        unzip -o BoostKit-omniruntime-omnioperator-*.zip
    else
        echo "Warning: No BoostKit-omniruntime-omnioperator-*.zip found in $TMP_DIR"
    fi

    if ls boostkit-omniop-operator-*.tar.gz 1>/dev/null 2>&1; then
        tar -zxvf boostkit-omniop-operator-*.tar.gz
    else
        echo "Warning: No boostkit-omniop-operator-*.tar.gz found in $TMP_DIR"
    fi

    find . \( -name "lib*" -o -name "*.jar" \) -type f -exec /bin/mv -n {} "$OMNI_HOME/lib/" \;

    echo "All operations completed successfully."
    ```

3. 执行安装脚本并设置权限。

    ```shell
    chmod +x /opt/install_gluten.sh
    /opt/install_gluten.sh

    chmod -R 550 /opt/omni-operator/*
    chmod 750 /opt/omni-operator/conf
    chmod 640 /opt/omni-operator/conf/omni.conf
    ```

### 安装后验证<a name="verify_installation"></a>

1. 验证环境变量是否生效。

    ```shell
    echo $OMNI_HOME
    ```

    预期输出：

    ```text
    /opt/omni-operator
    ```

2. 验证依赖库版本和插件是否就位。

    ```shell
    ls /opt/omni-operator/lib/libLLVM-15.so /opt/omni-operator/lib/libjemalloc.so.2 /opt/omni-operator/lib/libarrow_dataset.so.1100
    ls /opt/omni-operator/lib | grep -E "omniop|gluten"
    ```

    预期输出中除`libLLVM-15.so`、`libjemalloc.so.2`和`libarrow_dataset.so.1100`外，还包含对应版本的插件文件：SparkExtension场景包含`libboostkit-omniop-*-2.0.0-aarch64.so`等文件；Gluten场景包含`libspark_columnar_plugin.so`、`gluten-omni-bundle-*-2.2.0.jar`等文件。

## 快速开始<a name="quick_start_guide"></a>

本章通过TPC-DS测试集Q82对比启用OmniOperator算子加速前后的查询耗时。Q82涉及`item`、`inventory`、`date_dim`、`store_sales`四张ORC表，执行成功后返回1行结果。通过对比开源Spark与启用后的执行耗时和查询结果，验证加速效果。

**前提条件**：

- 已按[快速安装OmniOperator算子加速](#quick_install)完成对应场景的安装。
- Spark、Hadoop集群可正常运行，管理节点可提交Yarn任务。
- 已导入2GB（SF=2）TPC-DS数据集，并已确认实际数据库名。本文命令以`tpcds_bin_partitioned_decimal_orc_2`为例，如实际数据库名不同，请替换为实际值。数据集准备方法见[准备TPC-DS数据集](#prepare_tpcds_data)。

### 准备TPC-DS数据集<a name="prepare_tpcds_data"></a>

本文使用TPC-DS SF=2（约2GB）数据集。环境中已存在TPC-DS数据集时，可跳过本节，直接执行以下命令确认数据库名，并将其替换到后续命令中。

```shell
beeline -u jdbc:hive2://localhost:10000/default -e "show databases like 'tpcds%';"
```

未预置数据集时，直接下载已适配鲲鹏（aarch64）并完成预编译的hive-testbench压缩包导入，无需再编译数据生成器。

1. 下载并解压hive-testbench压缩包。

    ```shell
    cd /opt
    curl -fL -o hive-testbench-hdp3-arm.zip \
      https://gitcode.com/helloxteen_/OmniOperatorDebugAgent/releases/download/tpcds/hive-testbench-hdp3-arm.zip
    unzip -q -o hive-testbench-hdp3-arm.zip
    chmod +x /opt/hive-testbench-hdp3-arm/tpcds-setup.sh
    ```

    预期结果：解压目录`/opt/hive-testbench-hdp3-arm`中存在`tpcds-setup.sh`，以及预编译的`tpcds-gen/target/tpcds-gen-1.0-SNAPSHOT.jar`和`tpcds-gen/target/tools/dsdgen`。

2. 导入数据集。

    ```shell
    cd /opt/hive-testbench-hdp3-arm
    ./tpcds-setup.sh 2
    ```

    `tpcds-setup.sh`的参数为数据规模，`2`表示SF=2。脚本先在HDFS上生成24张表的文本数据，再导入为ORC分区表，数据库名为`tpcds_bin_partitioned_decimal_orc_2`，与本文后续命令一致。单节点执行约15分钟，耗时与集群规格相关。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >- 脚本通过`which hive`检查Hive是否已安装，执行前请确认Hive的`bin`目录已加入`PATH`，且HDFS、Yarn、HiveServer2已启动。
    >- 脚本使用`hive`命令连接元数据服务，请确认`hive-site.xml`中的`hive.metastore.uris`指向可用的元数据服务。
    >- 如需更改数据规模，替换参数`2`即可（需大于1）。

3. 确认数据库名和数据量。

    ```shell
    beeline -u jdbc:hive2://localhost:10000/default -e "show databases like 'tpcds%';"
    ```

    预期输出中包含`tpcds_bin_partitioned_decimal_orc_2`。

    ```sql
    use tpcds_bin_partitioned_decimal_orc_2;
    select count(*) from item;
    select count(*) from inventory;
    select count(*) from date_dim;
    select count(*) from store_sales;
    ```

    SF=2的参考行数为：`item` 26000行、`inventory` 16966305行、`date_dim` 73049行、`store_sales` 5760749行。四张表均有数据且量级相符即可继续后续步骤。

### 场景A：通过SparkExtension启用（Spark 3.3.1）<a name="scenario_sparkextension"></a>

1. 配置SparkExtension启动参数。

    进入`/usr/local/spark/conf`目录，创建`spark-defaults-omnioperator.conf`文件，并设置文件权限为640。

    ```shell
    cd /usr/local/spark/conf
    cp spark-defaults.conf spark-defaults-omnioperator.conf
    chmod 640 spark-defaults-omnioperator.conf
    ```

    在文件末尾追加以下参数。

    ```text
    spark.sql.optimizer.runtime.bloomFilter.enabled true
    spark.driverEnv.LD_LIBRARY_PATH /opt/omni-operator/lib
    spark.driverEnv.LD_PRELOAD /opt/omni-operator/lib/libjemalloc.so.2
    spark.driverEnv.OMNI_HOME /opt/omni-operator
    spark.driver.extraClassPath /opt/omni-operator/lib/boostkit-omniop-spark-3.3.1-2.0.0-aarch64.jar:/opt/omni-operator/lib/boostkit-omniop-bindings-2.0.0-aarch64.jar:/opt/omni-operator/lib/dependencies/protobuf-java-3.15.8.jar:/opt/omni-operator/lib/dependencies/boostkit-omniop-native-reader-3.3.1-2.0.0.jar
    spark.driver.extraLibraryPath /opt/omni-operator/lib
    spark.driver.defaultJavaOptions -Djava.library.path=/opt/omni-operator/lib
    spark.executorEnv.LD_LIBRARY_PATH ${PWD}/omni/omni-operator/lib
    spark.executorEnv.LD_PRELOAD ${PWD}/omni/omni-operator/lib/libjemalloc.so.2
    spark.executorEnv.MALLOC_CONF narenas:2
    spark.executorEnv.OMNI_HOME ${PWD}/omni/omni-operator
    spark.executor.extraClassPath ${PWD}/omni/omni-operator/lib/boostkit-omniop-spark-3.3.1-2.0.0-aarch64.jar:${PWD}/omni/omni-operator/lib/boostkit-omniop-bindings-2.0.0-aarch64.jar:${PWD}/omni/omni-operator/lib/dependencies/protobuf-java-3.15.8.jar:${PWD}/omni/omni-operator/lib/dependencies/boostkit-omniop-native-reader-3.3.1-2.0.0.jar
    spark.executor.extraLibraryPath ${PWD}/omni/omni-operator/lib
    spark.omni.sql.columnar.fusion false
    spark.shuffle.manager org.apache.spark.shuffle.sort.OmniColumnarShuffleManager
    spark.sql.codegen.wholeStage false
    spark.sql.extensions com.huawei.boostkit.spark.ColumnarPlugin
    spark.omni.sql.columnar.RewriteSelfJoinInInPredicate true
    spark.sql.execution.filterMerge.enabled true
    spark.omni.sql.columnar.dedupLeftSemiJoin true
    spark.omni.sql.columnar.radixSort.enabled true
    spark.executorEnv.MALLOC_CONF tcache:false
    spark.sql.adaptive.coalescePartitions.minPartitionNum 200
    spark.sql.join.columnar.preferShuffledHashJoin true
    ```

    执行后无报错即表示配置文件创建成功。

2. 将OmniOperator安装目录打包并上传至HDFS，供各计算节点分发使用。

    ```shell
    cd /opt
    tar -czvf /opt/omni-operator.tar.gz -C /opt omni-operator
    hdfs dfs -rm -r /user/root/omni-operator.tar.gz
    hdfs dfs -put /opt/omni-operator.tar.gz /user/root
    ```

    预期结果：执行`hdfs dfs -ls /user/root/omni-operator.tar.gz`可查询到该文件。

3. 启动Spark-SQL命令行窗口。

    ```shell
    /usr/local/spark/bin/spark-sql --archives hdfs://server1:9000/user/root/omni-operator.tar.gz#omni --deploy-mode client --driver-cores 8 --driver-memory 40g --master yarn --executor-cores 12 --executor-memory 5g --conf spark.memory.offHeap.enabled=true --conf spark.memory.offHeap.size=35g --num-executors 24 --conf spark.executor.extraJavaOptions='-XX:+UseG1GC' --conf spark.locality.wait=0 --conf spark.network.timeout=600 --conf spark.serializer=org.apache.spark.serializer.KryoSerializer --conf spark.sql.adaptive.enabled=true --conf spark.sql.adaptive.skewedJoin.enabled=true --conf spark.sql.autoBroadcastJoinThreshold=100M --conf spark.sql.broadcastTimeout=600 --conf spark.sql.shuffle.partitions=600 --conf spark.sql.orc.impl=native --conf spark.task.cpus=1 --properties-file /usr/local/spark/conf/spark-defaults-omnioperator.conf --database tpcds_bin_partitioned_decimal_orc_2
    ```

    启动成功后，命令行提示符变为：

    ```text
    spark-sql>
    ```

4. 验证SparkExtension是否生效。

    ```shell
    SET spark.sql.adaptive.enabled=false;
    EXPLAIN SELECT count(*) FROM item;
    ```

    预期输出中，执行计划包含以`Omni`开头的算子（如`OmniColumnarToRow`），表示SparkExtension已生效。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >启动参数中`spark.sql.adaptive.enabled=true`时，`EXPLAIN`可能只显示`AdaptiveSparkPlan`，看不到Omni算子。请先执行`SET spark.sql.adaptive.enabled=false;`再验证，或执行SQL后在Spark UI中查看实际执行计划。

5. 执行Q82并记录耗时。

    ```shell
    set spark.sql.adaptive.enabled=false;
    select i_item_id
        ,i_item_desc
        ,i_current_price
    from item, inventory, date_dim, store_sales
    where i_current_price between 76 and 76+30
    and inv_item_sk = i_item_sk
    and d_date_sk=inv_date_sk
    and d_date between cast('1998-06-29' as date) and cast('1998-08-29' as date)
    and i_manufact_id in (512,409,677,16)
    and inv_quantity_on_hand between 100 and 500
    and ss_item_sk = i_item_sk
    group by i_item_id,i_item_desc,i_current_price
    order by i_item_id
    limit 100;
    ```

    显示结果如下。

    ```text
    AAAAAAAAEEDAAAAA        Dead pages know mo        93.11
    Time taken: 15.066 seconds, Fetched 1 row(s)
    ```

6. （可选）获取对比基线：在不加载`spark-defaults-omnioperator.conf`的情况下，使用开源Spark执行同一SQL并记录耗时。预期结果：查询结果与启用后一致，执行耗时明显更长（示例环境为21.832秒），对比结果见[结果说明](#result_analysis)。

### 场景B：通过Gluten启用（Spark 3.3.1/3.4.3/3.5.2）<a name="scenario_gluten"></a>

1. 配置Gluten启动参数。

    进入`/usr/local/spark/conf`目录，创建`spark-defaults-omnioperator.conf`文件，并设置文件权限为640。

    ```shell
    cd /usr/local/spark/conf
    cp spark-defaults.conf spark-defaults-omnioperator.conf
    chmod 640 spark-defaults-omnioperator.conf
    ```

    在文件末尾追加以下参数（以Spark 3.5.2为例）。

    ```text
    spark.plugins org.apache.gluten.GlutenPlugin
    spark.shuffle.manager org.apache.spark.shuffle.sort.ColumnarShuffleManager
    spark.executor.memoryOverhead=3g
    spark.memory.offHeap.enabled true
    spark.memory.offHeap.size 35g
    spark.gluten.sql.columnar.backend.lib omni
    spark.executor.extraClassPath ${PWD}/omni/omni-operator/lib/gluten-omni-bundle-spark3.5_2.12-openEuler_22.03_aarch_64-1.3.0-2.2.0.jar:${PWD}/omni/omni-operator/lib/boostkit-omniop-bindings-2.2.0-aarch64.jar
    spark.driver.extraClassPath /opt/omni-operator/lib/gluten-omni-bundle-spark3.5_2.12-openEuler_22.03_aarch_64-1.3.0-2.2.0.jar:/opt/omni-operator/lib/boostkit-omniop-bindings-2.2.0-aarch64.jar
    spark.executorEnv.LD_LIBRARY_PATH ${PWD}/omni/omni-operator/lib
    spark.executorEnv.OMNI_HOME ${PWD}/omni/omni-operator
    spark.driverEnv.LD_LIBRARY_PATH /opt/omni-operator/lib
    spark.driverEnv.OMNI_HOME /opt/omni-operator
    spark.driverEnv.LD_PRELOAD /opt/omni-operator/lib/libjemalloc.so.2
    spark.executorEnv.LD_PRELOAD ${PWD}/omni/omni-operator/lib/libjemalloc.so.2
    spark.gluten.sql.columnar.libpath /opt/omni-operator/lib/libspark_columnar_plugin.so
    spark.gluten.sql.columnar.executor.libpath ${PWD}/omni/omni-operator/lib/libspark_columnar_plugin.so
    spark.gluten.sql.native.union true
    spark.gluten.sql.columnar.forceShuffledHashJoin true
    spark.sql.ansi.enabled false
    spark.executorEnv.MALLOC_CONF tcache:false
    spark.driverEnv.MALLOC_CONF tcache:false
    spark.sql.parquet.datetimeRebaseModeInRead CORRECTED
    spark.sql.parquet.int96RebaseModeInRead CORRECTED
    spark.gluten.sql.columnar.backend.omni.combineJoinedAggregates true
    spark.gluten.sql.columnar.backend.omni.joinReorderEnhance true
    spark.gluten.sql.columnar.backend.omni.dedupLeftSemiJoin true
    spark.gluten.sql.columnar.backend.omni.pushOrderedLimitThroughAggEnable true
    spark.gluten.sql.columnar.backend.omni.adaptivePartialAggregation true
    spark.gluten.sql.columnar.backend.omni.filterMerge true
    spark.gluten.sql.columnar.backend.omni.preferShuffledHashJoin true
    spark.gluten.sql.columnar.backend.omni.aggregationSpillEnabled false
    spark.gluten.sql.columnar.backend.omni.vec.predicate.enabled true
    spark.gluten.sql.columnar.backend.omni.rewriteSelfJoinInInPredicate true
    spark.gluten.sql.columnar.physicalJoinOptimizeEnable true
    spark.gluten.sql.columnar.physicalJoinOptimizationLevel 19
    spark.driver.maxResultSize 2G
    spark.network.timeout 600
    spark.serializer org.apache.spark.serializer.KryoSerializer
    spark.sql.adaptive.enabled true
    spark.sql.adaptive.skewedJoin.enabled true
    spark.sql.autoBroadcastJoinThreshold 100M
    spark.sql.broadcastTimeout 600
    spark.sql.shuffle.partitions 1000
    spark.sql.adaptive.coalescePartitions.minPartitionNum 400
    spark.sql.adaptive.coalescePartitions.initialPartitionNum 400
    spark.kryoserializer.buffer.max 1024m
    spark.reducer.maxSizeInFlight 128m
    spark.gluten.sql.columnar.maxBatchSize 8192
    spark.sql.orc.impl native
    spark.task.cpus 1
    spark.sql.sources.parallelPartitionDiscovery.parallelism 60
    ```

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >使用Spark 3.3.1或3.4.3时，`spark.driver.extraClassPath`和`spark.executor.extraClassPath`中的`gluten-omni-bundle-*.jar`、`boostkit-omniop-bindings-*.jar`需替换为对应版本安装包内的实际文件名。

    执行后无报错即表示配置文件创建成功。

2. 将OmniOperator安装目录打包并上传至HDFS。

    ```shell
    cd /opt
    tar -czvf /opt/omni-operator.tar.gz -C /opt omni-operator
    hdfs dfs -rm -r /user/root/omni-operator.tar.gz
    hdfs dfs -put /opt/omni-operator.tar.gz /user/root
    ```

    预期结果：执行`hdfs dfs -ls /user/root/omni-operator.tar.gz`可查询到该文件。

3. 启动Spark-SQL命令行窗口。

    ```shell
    /usr/local/spark/bin/spark-sql --archives hdfs://server1:9000/user/root/omni-operator.tar.gz#omni --deploy-mode client --driver-cores 8 --driver-memory 40g --master yarn --executor-cores 12 --executor-memory 5g --conf spark.memory.offHeap.enabled=true --conf spark.memory.offHeap.size=35g --num-executors 24 --conf spark.executor.extraJavaOptions='-XX:+UseG1GC' --conf spark.locality.wait=0 --conf spark.network.timeout=600 --conf spark.serializer=org.apache.spark.serializer.KryoSerializer --conf spark.sql.adaptive.enabled=true --conf spark.sql.adaptive.skewedJoin.enabled=true --conf spark.sql.autoBroadcastJoinThreshold=100M --conf spark.sql.broadcastTimeout=600 --conf spark.sql.shuffle.partitions=600 --conf spark.sql.orc.impl=native --conf spark.task.cpus=1 --properties-file /usr/local/spark/conf/spark-defaults-omnioperator.conf --database tpcds_bin_partitioned_decimal_orc_2
    ```

    启动成功后，命令行提示符变为：

    ```text
    spark-sql>
    ```

4. 验证Gluten是否生效。

    ```shell
    SET spark.sql.adaptive.enabled=false;
    EXPLAIN SELECT count(*) FROM item;
    ```

    预期输出中，执行计划包含以`Omni`开头的算子或以`Transformer`结尾的算子，表示Gluten已生效。

    >![说明](public_sys-resources/icon-note.gif) **说明：**
    >启动参数中`spark.sql.adaptive.enabled=true`时，`EXPLAIN`可能只显示`AdaptiveSparkPlan`，看不到Omni算子。请先执行`SET spark.sql.adaptive.enabled=false;`再验证，或执行SQL后在Spark UI中查看实际执行计划。

5. 执行Q82并记录耗时。

    ```shell
    set spark.sql.adaptive.enabled=false;
    select i_item_id
        ,i_item_desc
        ,i_current_price
    from item, inventory, date_dim, store_sales
    where i_current_price between 76 and 76+30
    and inv_item_sk = i_item_sk
    and d_date_sk=inv_date_sk
    and d_date between cast('1998-06-29' as date) and cast('1998-08-29' as date)
    and i_manufact_id in (512,409,677,16)
    and inv_quantity_on_hand between 100 and 500
    and ss_item_sk = i_item_sk
    group by i_item_id,i_item_desc,i_current_price
    order by i_item_id
    limit 100;
    ```

    显示结果如下。

    ```text
    AAAAAAAAEEDAAAAA        Dead pages know mo        93.11
    Time taken: 9.493 seconds, Fetched 1 row(s)
    ```

### 参数说明<a name="parameter_description"></a>

上述启动命令和配置文件中使用的主要参数如[**表 3**](#parameter_table)所示。

**表 3** 主要参数说明<a id="parameter_table"></a>

|参数|说明|
|--|--|
|--archives|将omni-operator.tar.gz分发到各Executor的工作目录，#omni表示解压后的目录名，配置文件中通过${PWD}/omni/omni-operator引用|
|--master yarn|使用Yarn作为集群资源管理器；单机模式可改为--master local|
|--deploy-mode client|Driver运行在提交任务的客户端|
|--driver-cores、--driver-memory|Driver使用的CPU核数和堆内存|
|--num-executors、--executor-cores、--executor-memory|Executor数量、每个Executor的CPU核数和堆内存，请根据集群规模调整|
|spark.memory.offHeap.enabled、spark.memory.offHeap.size|是否启用堆外内存及堆外内存大小。OmniOperator使用堆外内存存放列式数据，建议TPC-DS 3TB场景不低于20GB|
|spark.sql.shuffle.partitions|Shuffle分区数，影响并行度和单任务数据量|
|spark.sql.extensions|SparkExtension入口类，固定为com.huawei.boostkit.spark.ColumnarPlugin|
|spark.shuffle.manager|列式Shuffle管理器。SparkExtension为OmniColumnarShuffleManager，Gluten为ColumnarShuffleManager|
|spark.plugins、spark.gluten.sql.columnar.backend.lib|Gluten插件入口及后端选择，omni表示使用OmniOperator作为执行后端|
|spark.gluten.sql.columnar.libpath|Gluten Native库libspark_columnar_plugin.so的路径|
|spark.driver.extraClassPath、spark.executor.extraClassPath|插件JAR及依赖JAR的路径，多个JAR之间使用冒号分隔|
|spark.driverEnv.OMNI_HOME、spark.executorEnv.OMNI_HOME|Driver和Executor上的OmniOperator根目录|
|spark.driverEnv.LD_PRELOAD、spark.executorEnv.LD_PRELOAD|预加载jemalloc库，优化内存分配性能|

>![说明](public_sys-resources/icon-note.gif) **说明：**
>表 3 仅列出启动命令与配置文件中的主要参数。其余`spark.sql.*`、`spark.gluten.*`为Spark/Gluten通用调优参数，保持默认即可；需要调整时请参见《[用户指南](user_guide.md)》中的参数说明。

### 结果说明<a name="result_analysis"></a>

Q82执行完成后，spark-sql会在结果末尾输出`Time taken: xx seconds`，也可在Spark UI的SQL页面查看执行耗时。参考环境（openEuler 22.03 LTS、鲲鹏920、24个Executor、Driver 40GB，启动参数见[场景A](#scenario_sparkextension)）的对比结果如[**表 4**](#result_table)所示。

**表 4** Q82执行耗时对比<a id="result_table"></a>

|执行方式|Q82耗时（秒）|耗时降低比例|加速比|
|--|--|--|--|
|开源Spark|21.832|-|-|
|SparkExtension|15.066|31.0%|1.45|
|Gluten|9.493|56.5%|2.30|

结果显示：

- 三种方式的查询结果完全一致（均为1行，首行内容为`AAAAAAAAEEDAAAAA Dead pages know mo 93.11`），说明启用OmniOperator算子加速不会改变查询结果。
- 启用SparkExtension后Q82耗时由21.832秒降至15.066秒，降低约31.0%；启用Gluten后降至9.493秒，降低约56.5%。
- 实际耗时受集群规格、数据量、并发度和参数配置影响，表中数据仅用于说明加速效果，请以实际环境测试结果为准。单节点、少Executor环境下绝对耗时会更低，但加速比例接近。

## 更多功能<a name="more_features"></a>

- 如需了解支持的算子、表达式与数据类型范围，以及全部调优参数，请参见《[用户指南](user_guide.md)》。
- 如需了解多节点组网规划、完整安装步骤与软件包校验方法，请参见《[安装指南](installation_guide.md)》。
- 如需了解版本配套关系与各版本特性变更，请参见《[版本说明书](release_notes.md)》。
- 如需排查安装和使用中的常见问题，请参见《[常见问题](faq.md)》。
- 如需了解TPC-DS 10TB数据集下OmniOperator与Velox的性能对比数据，请参见《[项目介绍](../../README.md)》中的“性能数据”章节。
- 如需从源码编译OmniOperator和Gluten Omni后端，或了解Gluten侧更多启动参数，请参见[Gluten仓库](https://gitcode.com/openeuler/Gluten)中的编译命令与快速上手说明。

## 常见问题<a name="faq"></a>

### 如何确认OmniOperator算子加速已经生效<a name="faq_q1"></a>

#### 问题

执行SQL后无法判断查询是否真正走了Native算子，担心实际仍由开源Spark执行。

#### 回答

在SQL语句前加`EXPLAIN`查看执行计划：

- SparkExtension场景：执行计划中出现以`Omni`开头的算子，表示已生效。
- Gluten场景：执行计划中出现以`Omni`开头或以`Transformer`结尾的算子，表示已生效。

也可以打开Spark UI，在SQL页面的执行计划中查看算子名称。若没有任何Omni算子，请检查`spark.sql.extensions`（SparkExtension）或`spark.plugins`（Gluten）是否配置正确。

### 为什么部分算子没有使用Omni实现<a name="faq_q2"></a>

#### 问题

执行计划中只有部分算子以`Omni`开头，其他算子仍为开源实现。

#### 回答

OmniOperator仅替换其支持的算子与表达式，遇到不支持的场景时会自动回退为开源算子，属于正常现象。常见回退原因包括：

- 使用了不支持的算子、表达式或数据类型，支持范围请参见《[用户指南](user_guide.md)》中的“支持算子列表”和“支持表达式列表”。
- 多列表达式Codegen场景（如500列），编译开销可能大于加速收益，建议使用开源版本执行。
- Boolean类型比较运算符等明确不支持的场景，详见《[项目介绍](../../README.md)》中的“约束与限制”。

如果大表Join等关键算子发生回退，行列转换开销可能导致性能下降，建议调整SQL或数据类型，避免回退。

### SQL执行时报堆外内存不足或结果不正确，如何处理<a name="faq_q3"></a>

#### 问题

执行TPC-DS等大规模SQL时出现堆外内存不足报错（如`MEM_CAP_EXCEEDED`），或查询结果与开源Spark不一致。

#### 回答

OmniOperator使用堆外内存存放列式数据，请按以下顺序调整：

1. 增大`spark.memory.offHeap.size`，TPC-DS 3TB场景建议不低于20GB。
2. 适当增大`--executor-memory`和`spark.executor.memoryOverhead`。
3. 根据数据量调整`spark.sql.shuffle.partitions`，避免单分区数据量过大。
4. 若日志出现`MEM_CAP_EXCEEDED`但SQL最终执行成功，通常不影响结果，可继续观察；若结果异常，优先检查堆外内存是否配置过低。

### 不同Spark版本应选择哪种启用方式<a name="faq_q4"></a>

#### 问题

环境中存在多个Spark版本，不确定应使用SparkExtension还是Gluten。

#### 回答

- Spark 3.3.1、3.4.3、3.5.2：建议优先使用Gluten方式，但要求服务器支持SVE指令集，且操作系统为openEuler 22.03或24.03。
- 其他Spark版本（如3.1.1）：使用SparkExtension方式，支持CentOS 7.9、openEuler 20.03和22.03。
- 可通过`spark-sql --version`确认Spark版本，通过`cat /proc/cpuinfo | grep sve | head -n 1`确认是否支持SVE。

## 故障排除<a name="troubleshooting"></a>

### 案例1：启动Spark-SQL时提示找不到libjemalloc.so.2<a name="trouble_case1"></a>

- **问题描述**：启动spark-sql时报错`error while loading shared libraries: libjemalloc.so.2: cannot open shared object file: No such file or directory`，任务无法启动。
- **对系统的影响**：Driver或Executor进程无法启动，SQL无法执行。
- **可能原因**：`libjemalloc.so.2`未拷贝到`/opt/omni-operator/lib`，或`spark.driverEnv.LD_PRELOAD`、`spark.executorEnv.LD_PRELOAD`路径配置错误。
- **排查思路（可选）**：先确认文件是否存在，再核对Driver和Executor的路径配置。
- **操作步骤**：
    1. 执行以下命令确认文件存在。

        ```shell
        ls -l /opt/omni-operator/lib/libjemalloc.so.2
        ```

    2. 确认`spark-defaults-omnioperator.conf`中`LD_PRELOAD`的Driver路径为`/opt/omni-operator/lib/libjemalloc.so.2`，Executor路径为`${PWD}/omni/omni-operator/lib/libjemalloc.so.2`。
    3. 重新启动spark-sql，预期不再出现该报错。

### 案例2：启动时报UnsatisfiedLinkError或找不到Native库<a name="trouble_case2"></a>

- **问题描述**：启动spark-sql或执行SQL时报`java.lang.UnsatisfiedLinkError`，或提示`no omni in java.library.path`。
- **对系统的影响**：Native算子无法加载，任务启动失败或全部回退为开源算子。
- **可能原因**：`--archives`未正确分发安装包；`spark.driver.extraClassPath`、`spark.executor.extraClassPath`中的JAR名称与实际不符；Gluten场景`spark.gluten.sql.columnar.libpath`或`spark.gluten.sql.columnar.executor.libpath`配置错误。
- **排查思路（可选）**：按“归档是否分发→JAR名称是否正确→Native库路径是否正确”逐层排查。
- **操作步骤**：
    1. 确认HDFS上的安装包存在。

        ```shell
        hdfs dfs -ls /user/root/omni-operator.tar.gz
        ```

    2. 确认`/opt/omni-operator/lib`下存在实际使用的JAR和`libspark_columnar_plugin.so`（Gluten场景）。

        ```shell
        ls /opt/omni-operator/lib | grep -E "gluten-omni-bundle|bindings|libspark_columnar_plugin"
        ```

    3. 将配置文件中`extraClassPath`的JAR名称替换为上述命令输出的实际文件名，重新启动spark-sql。

    预期结果：spark-sql启动成功，再次执行`EXPLAIN`时出现Omni算子，不再报`UnsatisfiedLinkError`。

### 案例3：Gluten场景执行计划中没有Omni算子<a name="trouble_case3"></a>

- **问题描述**：已按Gluten方式启动spark-sql，但`EXPLAIN`输出中没有以`Omni`开头或以`Transformer`结尾的算子。
- **对系统的影响**：查询仍由开源Spark执行，无法获得加速效果。
- **可能原因**：服务器不支持SVE指令集；`spark.gluten.sql.columnar.backend.lib`未设置为`omni`；`spark.plugins`未配置或JAR未加载。
- **排查思路（可选）**：先确认硬件支持SVE，再检查Gluten插件配置是否完整生效。
- **操作步骤**：
    1. 执行以下命令确认SVE支持情况。

        ```shell
        cat /proc/cpuinfo | grep sve | head -n 1
        ```

        无输出表示不支持SVE，请改用SparkExtension方式。

    2. 确认`spark-defaults-omnioperator.conf`中包含以下配置。

        ```text
        spark.plugins org.apache.gluten.GlutenPlugin
        spark.gluten.sql.columnar.backend.lib omni
        spark.gluten.sql.columnar.libpath /opt/omni-operator/lib/libspark_columnar_plugin.so
        ```

    3. 重新启动spark-sql并执行`EXPLAIN SELECT count(*) FROM item;`，预期出现`Omni`前缀或`Transformer`结尾的算子。

## 修订记录<a name="revision_history"></a>

|文档版本|发布日期|修改说明|
|--|--|--|
|01|2026-12-30|第一次正式发布。|
