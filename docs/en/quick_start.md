# Quick Start<a name="quick_start"></a>

This document describes how to quickly install and configure OmniOperator acceleration with SparkExtension or Gluten, and uses TPC-DS Q82 to verify the query performance improvement.

## OmniOperator Introduction<a name="omni_operator_introduction"></a>

OmniOperator is a subfeature of OmniRuntime, a big data feature of Kunpeng BoostKit. It implements big data SQL operators in native code (C/C++) and uses OmniVec columnar memory organization (zero-copy with no serialization overhead) and vectorized execution to replace Java operators in engines such as Spark and Hive with high-performance native operators, thereby improving SQL query performance. It is suitable for large-scale data fusion analysis scenarios and has been adapted to Spark 3.1.1/3.3.1/3.4.3/3.5.2, Hive 3.1.0, and Gluten 1.3. This document uses TPC-DS query Q82 as an example to describe how to quickly install, configure, and verify OmniOperator acceleration through SparkExtension and Gluten, helping you get started quickly and observe query performance improvements.

## Quick Installation of OmniOperator<a name="quick_install"></a>

This document describes how to quickly install OmniOperator. For details about network planning and multi-node deployment, see the [Installation Guide](installation_guide.md).

### Preparations<a name="installation_preparation"></a>

All installation commands in this section require root permissions. Run them as `sudo` or switch to the root user. A storage-compute coupled networking architecture is recommended: one management node (server1) submits tasks, and three compute nodes (agent1 to agent3) run OmniOperator. In single-node mode, perform all the following operations on the same node.

Run the following commands to check the environment. Continue the installation only after all commands succeed and the results meet expectations.

**Table 1** Environment check<a id="environment_check"></a>

|Check Item|Command|Expected Result|Failure Handling|
|--|--|--|--|
|OS check|cat /etc/os-release|For Gluten: openEuler 22.03 LTS SP4 (other 22.03 patch versions are also supported) or openEuler 24.03 LTS SP1. For SparkExtension: CentOS 7.9, openEuler 20.03 LTS SP1, or openEuler 22.03 LTS SP4 (other 22.03 patch versions are also supported).|Change to a supported OS version.|
|SVE instruction set check (Gluten)|cat /proc/cpuinfo \| grep sve \| head -n 1|Any output indicates that the CPU supports the SVE instruction set.|Use a Kunpeng server that supports SVE, or use SparkExtension instead.|
|JDK check|java -version|BiSheng JDK 1.8.0_342 (openEuler 22.03) or JDK 17 (openEuler 24.03)|Install the required JDK version and configure JAVA_HOME.|
|Hadoop check|hdfs dfs -ls /|The HDFS root directory is listed.|Start the HDFS service or check fs.defaultFS in core-site.xml.|
|Spark check|spark-sql --version|For Gluten: 3.3.1, 3.4.3, or 3.5.2. For SparkExtension: 3.1.1, 3.3.1, 3.4.3, or 3.5.2.|Install the corresponding Spark version and configure SPARK_HOME.|
|Memory check|free -h|Available memory is at least 32 GB (384 GB recommended).|Stop services that consume memory or scale up the memory.|
|Disk check|df -h /opt|At least 50 GB is available in /opt.|Clean up the disk or change the installation directory.|

>![Note](public_sys-resources/icon-note.gif) **NOTE:**
>
>- Hardware requirements: Kunpeng 920 or Kunpeng 950 processor, and 384 GB memory (12 x 32 GB) is recommended.
>- This document uses openEuler 22.03 LTS SP4 as the hands-on environment and Spark 3.3.1/3.5.2 and OmniOperator 2.2.0 as examples. Other patch versions of openEuler 22.03 (for example, SP1, SP2, and SP3) use the same steps. For other supported versions, only the version numbers in the package names differ.

### Preparing Environment Dependencies<a name="prepare_environment"></a>

1. Install basic tools.

    ```shell
    yum install -y tar unzip gzip xz curl wget which findutils procps-ng
    ```

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >The openEuler base image does not include `unzip` by default. If it is not installed, decompressing packages later fails with `unzip: command not found`.

2. Configure the dynamic library search path.

    ```shell
    cat > /etc/ld.so.conf.d/usr-local.conf <<'EOF'
    /usr/local/lib
    /usr/local/lib64
    EOF
    ldconfig
    ```

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >This document uses precompiled packages; the GCC, CMake, and Maven toolchains are not required for the SparkExtension scenario. The Gluten scenario requires GCC/G++, CMake, and Make to compile ABSL. To compile OmniOperator from source, see the source compilation guide for the complete system preparation and dependency installation steps.

### Obtaining Software Packages<a name="obtain_packages"></a>

Obtain the installation packages from the Kunpeng community or the code hosting platform, and select them based on your scenario. [**Table 2**](#software_packages) lists the packages used in this document.

**Table 2** Software packages<a id="software_packages"></a>

|Software Type|Required/Optional|Description|Package Name|Download Link|
|--|--|--|--|--|
|OmniRuntime package|Required for SparkExtension|Contains the OmniOperator installation package after decompression|BoostKit-omniruntime_1.9.0.zip|[OmniRuntime package download](https://kunpeng-repo.obs.cn-north-4.myhuaweicloud.com/Kunpeng%20BoostKit/Kunpeng%20BoostKit%2025.3.0/BoostKit-omniruntime_1.9.0.zip)|
|SparkExtension|Required for SparkExtension|Spark engine extension package. Select the version based on the Spark version.|boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip|[SparkExtension download](https://gitcode.com/boostkit/boostkit-bigdata/releases/download/Kunpeng-BoostKit-25.3.0-OmniOperator-release/boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip)|
|Dependency_library|Required for SparkExtension|Runtime dependency library. Select the version based on the OS.|Dependency_library_openeuler22.03.zip|[Dependency_library download](https://gitcode.com/boostkit/boostkit-bigdata/releases/download/Kunpeng-BoostKit-25.3.0-OmniOperator-release/Dependency_library_openeuler22.03.zip)|
|Gluten|Required for Gluten|Gluten adaptation package. Select the version based on the Spark version: Spark 3.5.2 uses 2.2.0, Spark 3.4.3 uses 2.0.0, and Spark 3.3.1 uses 1.0.0.|BoostKit-omniruntime-gluten-2.2.0.zip|[Gluten download](https://atomgit.com/openeuler/OmniOperator/releases/download/26.1.0-OmniOperator-2.2.0-release/BoostKit-omniruntime-gluten-2.2.0.zip)|
|Dependency_library_Gluten|Required for Gluten|Runtime dependency library for Gluten|Dependency_library_Gluten.zip|[Dependency_library_Gluten download](https://atomgit.com/openeuler/OmniOperator/releases/download/26.1.0-OmniOperator-2.2.0-release/Dependency_library_Gluten.zip)|

>![Note](public_sys-resources/icon-note.gif) **NOTE:**
>
>- Before using the packages, read the [Kunpeng Application Enablement Kit BoostKit User License Agreement 2.0](https://www.hikunpeng.com/zh/legal/developer/boostkit/software/protocol).
>- For SparkExtension, also install the extension package that matches the Spark version. Gluten supports only Spark 3.3.1, 3.4.3, and 3.5.2.

### Installing Runtime Dependencies<a name="install_dependencies"></a>

1. Install OS dependencies.

    ```shell
    yum install lz4-devel zstd-devel snappy-devel protobuf-c-devel protobuf-lite-devel \
      boost-devel cyrus-sasl-devel jsoncpp-devel openssl-devel libatomic -y

    # Additional dependencies for Gluten
    yum install double-conversion* gflags* glog* libaio numactl-libs -y
    ```

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >If the `yum` repository is unavailable, configure a local Yum repository first. The following uses openEuler 22.03 LTS SP4 as an example. Run the following command:
    >
    >```shell
    >dnf config-manager --add-repo https://repo.oepkgs.net/openeuler/rpm/openEuler-22.03-LTS-SP4/extras/aarch64/
    >```

2. Deploy LLVM and jemalloc.

    ```shell
    mkdir -p /opt/omni-operator/lib
    cd /opt/omni-operator

    # For SparkExtension, change to DEP_DIR=Dependency_library_openeuler22.03
    DEP_DIR=Dependency_library_Gluten
    unzip ${DEP_DIR}.zip
    cp ${DEP_DIR}/* /opt/omni-operator/lib/
    ```

    >![Notice](public_sys-resources/icon-notice.gif) **NOTICE:**
    >- You must copy all library files in the `Dependency_library` directory, including the Arrow, Parquet, and ORC libraries. If only `libLLVM-15.so` and `libjemalloc.so.2` are copied, ORC read and write operations fail with `libarrow_dataset.so.1100: cannot open shared object file`.
    >- If LLVM and jemalloc have been installed in the environment, delete the existing `libLLVM-15.so` and `libjemalloc.so.2` before copying the new files.

3. (Gluten only) Compile and install the ABSL library.

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

4. Configure environment variables.

    ```shell
    echo "export OMNI_HOME=/opt/omni-operator" >> ~/.bashrc
    echo 'export LD_LIBRARY_PATH=$OMNI_HOME/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
    source ~/.bashrc
    ```

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >If `LD_LIBRARY_PATH` does not exist in the environment, set it to `export LD_LIBRARY_PATH=$OMNI_HOME/lib` to prevent the current directory from being added to the dynamic library search path.

### Installing OmniOperator Packages<a name="install_omnioperator"></a>

Select one of the following installation methods based on how you want to enable OmniOperator.

>![Note](public_sys-resources/icon-note.gif) **NOTE:**
>
>- If `Permission denied` is displayed, confirm that the current user is root. If a dependency is missing, return to [Installing Runtime Dependencies](#install_dependencies) to install it.
>- If `No such file or directory` is displayed, confirm that the package has been uploaded to the target directory and that the version number in the package name matches the actual file.

#### Method 1: Installing Packages for SparkExtension<a name="install_sparkextension_package"></a>

1. Upload `BoostKit-omniruntime_1.9.0.zip`, `BoostKit-omniop_2.0.0.zip`, and the SparkExtension package to `/opt/omni-operator`, and decompress them.

    ```shell
    cd /opt/omni-operator
    unzip BoostKit-omniruntime_1.9.0.zip
    unzip BoostKit-omniop_2.0.0.zip
    tar -zxvf boostkit-omniop-operator-2.0.0-aarch64-openeuler.tar.gz
    ```

2. Copy the library and header files, and set permissions.

    ```shell
    cd /opt/omni-operator/boostkit-omniop-operator-2.0.0-aarch64
    cp -r include libboostkit* boostkit-omniop* libsecurec.so /opt/omni-operator/lib/
    chmod -R 550 /opt/omni-operator/lib/*
    ```

3. Create the configuration directory and file.

    ```shell
    mkdir -p /opt/omni-operator/conf
    touch /opt/omni-operator/conf/omni.conf
    chmod 750 /opt/omni-operator/conf
    chmod 640 /opt/omni-operator/conf/omni.conf
    ```

4. Deploy the SparkExtension package.

    ```shell
    cd /opt/omni-operator
    unzip boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip
    unzip boostkit-omniop-spark-3.3.1-2.0.0-aarch64-openeuler.zip
    mv boostkit-omniop-spark-3.3.1-2.0.0-aarch64.jar ./lib/
    tar -zxvf dependencies.tar.gz -C ./lib
    ```

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >`boostkit-omniop-spark-3.3.1-2.0.0-aarch64.zip` contains a NEON implementation (`-openeuler.zip`) and an SVE implementation (`-openeuler-sve.zip`). Select the package based on whether the server supports the SVE instruction set. This document uses the NEON implementation as an example.

#### Method 2: Installing Packages for Gluten<a name="install_gluten_package"></a>

1. Upload `BoostKit-omniruntime-gluten-2.2.0.zip` and `Dependency_library_Gluten.zip` to `/opt`.

2. Create the installation script `/opt/install_gluten.sh` and add the following content.

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

3. Run the installation script and set permissions.

    ```shell
    chmod +x /opt/install_gluten.sh
    /opt/install_gluten.sh

    chmod -R 550 /opt/omni-operator/*
    chmod 750 /opt/omni-operator/conf
    chmod 640 /opt/omni-operator/conf/omni.conf
    ```

### Verifying the Installation<a name="verify_installation"></a>

1. Verify that the environment variable takes effect.

    ```shell
    echo $OMNI_HOME
    ```

    Expected output:

    ```text
    /opt/omni-operator
    ```

2. Verify the dependency library version and the plug-in files.

    ```shell
    ls /opt/omni-operator/lib/libLLVM-15.so /opt/omni-operator/lib/libjemalloc.so.2 /opt/omni-operator/lib/libarrow_dataset.so.1100
    ls /opt/omni-operator/lib | grep -E "omniop|gluten"
    ```

    In addition to `libLLVM-15.so`, `libjemalloc.so.2`, and `libarrow_dataset.so.1100`, the output should contain the plug-in files of the corresponding version. For SparkExtension, the output contains files such as `libboostkit-omniop-*-2.0.0-aarch64.so`. For Gluten, the output contains files such as `libspark_columnar_plugin.so` and `gluten-omni-bundle-*-2.2.0.jar`.

## Quick Start Example<a name="quick_start_guide"></a>

This chapter uses TPC-DS query Q82 to compare the query execution time before and after enabling OmniOperator. Q82 involves four ORC tables (`item`, `inventory`, `date_dim`, and `store_sales`) and returns one row after a successful execution. Compare the execution time and query results between open-source Spark and the accelerated Spark to verify the acceleration effect.

**Prerequisites:**

- You have completed the installation for the target scenario by following [Quick Installation of OmniOperator](#quick_install).
- The Spark and Hadoop clusters are running properly, and the management node can submit YARN tasks.
- The 2 GB (SF=2) TPC-DS dataset has been imported, and the actual database name has been confirmed. This document uses `tpcds_bin_partitioned_decimal_orc_2` as an example. If your database name is different, replace it with the actual name. For details about dataset preparation, see [Preparing the TPC-DS Dataset](#prepare_tpcds_data).

### Preparing the TPC-DS Dataset<a name="prepare_tpcds_data"></a>

This document uses the TPC-DS SF=2 (about 2 GB) dataset. If a TPC-DS dataset already exists in your environment, skip this section, run the following command to obtain the database name, and replace the database name in subsequent commands.

```shell
beeline -u jdbc:hive2://localhost:10000/default -e "show databases like 'tpcds%';"
```

If no dataset is available, directly download the prebuilt hive-testbench package for Kunpeng (aarch64) and import the data. No compilation is required.

1. Download and decompress the hive-testbench package.

    ```shell
    cd /opt
    curl -fL -o hive-testbench-hdp3-arm.zip \
      https://gitcode.com/helloxteen_/OmniOperatorDebugAgent/releases/download/tpcds/hive-testbench-hdp3-arm.zip
    unzip -q -o hive-testbench-hdp3-arm.zip
    chmod +x /opt/hive-testbench-hdp3-arm/tpcds-setup.sh
    ```

    Expected result: the `/opt/hive-testbench-hdp3-arm` directory contains `tpcds-setup.sh`, the prebuilt `tpcds-gen/target/tpcds-gen-1.0-SNAPSHOT.jar`, and `tpcds-gen/target/tools/dsdgen`.

2. Import the dataset.

    ```shell
    cd /opt/hive-testbench-hdp3-arm
    ./tpcds-setup.sh 2
    ```

    The parameter of `tpcds-setup.sh` is the data scale factor, and `2` indicates SF=2. The script first generates text data for 24 tables on HDFS, and then imports the data into partitioned ORC tables. The database name is `tpcds_bin_partitioned_decimal_orc_2`, which matches the commands in this document. On a single node, the process takes about 15 minutes, depending on the cluster specifications.

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >- The script runs `which hive` to check whether Hive is installed. Before running the script, ensure that the Hive `bin` directory is added to `PATH` and that HDFS, YARN, and HiveServer2 are started.
    >- The script uses the `hive` command to connect to the metastore. Ensure that `hive.metastore.uris` in `hive-site.xml` points to an available metastore service.
    >- To change the data scale, replace the parameter `2` with another value greater than 1.

3. Confirm the database name and data volume.

    ```shell
    beeline -u jdbc:hive2://localhost:10000/default -e "show databases like 'tpcds%';"
    ```

    The expected output contains `tpcds_bin_partitioned_decimal_orc_2`.

    ```sql
    use tpcds_bin_partitioned_decimal_orc_2;
    select count(*) from item;
    select count(*) from inventory;
    select count(*) from date_dim;
    select count(*) from store_sales;
    ```

    The reference row counts for SF=2 are as follows: `item` 26000, `inventory` 16966305, `date_dim` 73049, and `store_sales` 5760749. If all four tables contain data in the expected order of magnitude, proceed to the following steps.

### Scenario A: Enabling OmniOperator Through SparkExtension (Spark 3.3.1)<a name="scenario_sparkextension"></a>

1. Configure the SparkExtension startup parameters.

    Go to `/usr/local/spark/conf`, create the `spark-defaults-omnioperator.conf` file, and set the file permission to 640.

    ```shell
    cd /usr/local/spark/conf
    cp spark-defaults.conf spark-defaults-omnioperator.conf
    chmod 640 spark-defaults-omnioperator.conf
    ```

    Append the following parameters to the end of the file.

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

    If no error is reported, the configuration file is created successfully.

2. Package the OmniOperator installation directory and upload it to HDFS so that it can be distributed to compute nodes.

    ```shell
    cd /opt
    tar -czvf /opt/omni-operator.tar.gz -C /opt omni-operator
    hdfs dfs -rm -r /user/root/omni-operator.tar.gz
    hdfs dfs -put /opt/omni-operator.tar.gz /user/root
    ```

    Expected result: `hdfs dfs -ls /user/root/omni-operator.tar.gz` lists the file.

3. Start the Spark-SQL CLI.

    ```shell
    /usr/local/spark/bin/spark-sql --archives hdfs://server1:9000/user/root/omni-operator.tar.gz#omni --deploy-mode client --driver-cores 8 --driver-memory 40g --master yarn --executor-cores 12 --executor-memory 5g --conf spark.memory.offHeap.enabled=true --conf spark.memory.offHeap.size=35g --num-executors 24 --conf spark.executor.extraJavaOptions='-XX:+UseG1GC' --conf spark.locality.wait=0 --conf spark.network.timeout=600 --conf spark.serializer=org.apache.spark.serializer.KryoSerializer --conf spark.sql.adaptive.enabled=true --conf spark.sql.adaptive.skewedJoin.enabled=true --conf spark.sql.autoBroadcastJoinThreshold=100M --conf spark.sql.broadcastTimeout=600 --conf spark.sql.shuffle.partitions=600 --conf spark.sql.orc.impl=native --conf spark.task.cpus=1 --properties-file /usr/local/spark/conf/spark-defaults-omnioperator.conf --database tpcds_bin_partitioned_decimal_orc_2
    ```

    After the CLI starts, the command prompt changes to:

    ```text
    spark-sql>
    ```

4. Verify that SparkExtension is enabled.

    ```shell
    SET spark.sql.adaptive.enabled=false;
    EXPLAIN SELECT count(*) FROM item;
    ```

    The expected execution plan contains operators starting with `Omni` (for example, `OmniColumnarToRow`), indicating that SparkExtension is enabled.

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >When `spark.sql.adaptive.enabled=true` is set in the startup parameters, `EXPLAIN` may show only `AdaptiveSparkPlan` and no Omni operators. Run `SET spark.sql.adaptive.enabled=false;` before verification, or check the actual execution plan in the Spark UI after running the SQL statement.

5. Run Q82 and record the execution time.

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

    The output is as follows:

    ```text
    AAAAAAAAEEDAAAAA        Dead pages know mo        93.11
    Time taken: 15.066 seconds, Fetched 1 row(s)
    ```

6. (Optional) Obtain a baseline: run the same SQL with open-source Spark without loading `spark-defaults-omnioperator.conf`, and record the execution time. Expected result: the query result is the same as that after acceleration, and the execution time is significantly longer (21.832 seconds in the example environment). For details, see [Result Description](#result_analysis).

### Scenario B: Enabling OmniOperator Through Gluten (Spark 3.3.1/3.4.3/3.5.2)<a name="scenario_gluten"></a>

1. Configure the Gluten startup parameters.

    Go to `/usr/local/spark/conf`, create the `spark-defaults-omnioperator.conf` file, and set the file permission to 640.

    ```shell
    cd /usr/local/spark/conf
    cp spark-defaults.conf spark-defaults-omnioperator.conf
    chmod 640 spark-defaults-omnioperator.conf
    ```

    Append the following parameters to the end of the file (Spark 3.5.2 is used as an example).

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

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >When Spark 3.3.1 or 3.4.3 is used, replace `gluten-omni-bundle-*.jar` and `boostkit-omniop-bindings-*.jar` in `spark.driver.extraClassPath` and `spark.executor.extraClassPath` with the actual file names in the corresponding version package.

    If no error is reported, the configuration file is created successfully.

2. Package the OmniOperator installation directory and upload it to HDFS.

    ```shell
    cd /opt
    tar -czvf /opt/omni-operator.tar.gz -C /opt omni-operator
    hdfs dfs -rm -r /user/root/omni-operator.tar.gz
    hdfs dfs -put /opt/omni-operator.tar.gz /user/root
    ```

    Expected result: `hdfs dfs -ls /user/root/omni-operator.tar.gz` lists the file.

3. Start the Spark-SQL CLI.

    ```shell
    /usr/local/spark/bin/spark-sql --archives hdfs://server1:9000/user/root/omni-operator.tar.gz#omni --deploy-mode client --driver-cores 8 --driver-memory 40g --master yarn --executor-cores 12 --executor-memory 5g --conf spark.memory.offHeap.enabled=true --conf spark.memory.offHeap.size=35g --num-executors 24 --conf spark.executor.extraJavaOptions='-XX:+UseG1GC' --conf spark.locality.wait=0 --conf spark.network.timeout=600 --conf spark.serializer=org.apache.spark.serializer.KryoSerializer --conf spark.sql.adaptive.enabled=true --conf spark.sql.adaptive.skewedJoin.enabled=true --conf spark.sql.autoBroadcastJoinThreshold=100M --conf spark.sql.broadcastTimeout=600 --conf spark.sql.shuffle.partitions=600 --conf spark.sql.orc.impl=native --conf spark.task.cpus=1 --properties-file /usr/local/spark/conf/spark-defaults-omnioperator.conf --database tpcds_bin_partitioned_decimal_orc_2
    ```

    After the CLI starts, the command prompt changes to:

    ```text
    spark-sql>
    ```

4. Verify that Gluten is enabled.

    ```shell
    SET spark.sql.adaptive.enabled=false;
    EXPLAIN SELECT count(*) FROM item;
    ```

    The expected execution plan contains operators starting with `Omni` or ending with `Transformer`, indicating that Gluten is enabled.

    >![Note](public_sys-resources/icon-note.gif) **NOTE:**
    >When `spark.sql.adaptive.enabled=true` is set in the startup parameters, `EXPLAIN` may show only `AdaptiveSparkPlan` and no Omni operators. Run `SET spark.sql.adaptive.enabled=false;` before verification, or check the actual execution plan in the Spark UI after running the SQL statement.

5. Run Q82 and record the execution time.

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

    The output is as follows:

    ```text
    AAAAAAAAEEDAAAAA        Dead pages know mo        93.11
    Time taken: 9.493 seconds, Fetched 1 row(s)
    ```

### Parameter Description<a name="parameter_description"></a>

The main parameters used in the preceding startup commands and configuration files are described in [**Table 3**](#parameter_table).

**Table 3** Main parameters<a id="parameter_table"></a>

|Parameter|Description|
|--|--|
|--archives|Distributes omni-operator.tar.gz to the working directory of each executor. #omni specifies the name of the decompressed directory, which is referenced by ${PWD}/omni/omni-operator in the configuration file.|
|--master yarn|Uses YARN as the cluster resource manager. In single-node mode, change it to --master local.|
|--deploy-mode client|Runs the driver on the client that submits the task.|
|--driver-cores, --driver-memory|Number of CPU cores and heap memory for the driver.|
|--num-executors, --executor-cores, --executor-memory|Number of executors, CPU cores per executor, and heap memory per executor. Adjust them based on the cluster scale.|
|spark.memory.offHeap.enabled, spark.memory.offHeap.size|Whether to enable off-heap memory and the off-heap memory size. OmniOperator uses off-heap memory to store columnar data. For TPC-DS 3 TB, at least 20 GB is recommended.|
|spark.sql.shuffle.partitions|Number of shuffle partitions, which affects the parallelism and the data volume per task.|
|spark.sql.extensions|Entry class for SparkExtension, which is fixed to com.huawei.boostkit.spark.ColumnarPlugin.|
|spark.shuffle.manager|Columnar shuffle manager. Use OmniColumnarShuffleManager for SparkExtension and ColumnarShuffleManager for Gluten.|
|spark.plugins, spark.gluten.sql.columnar.backend.lib|Gluten plug-in entry and backend selection. omni indicates that OmniOperator is used as the execution backend.|
|spark.gluten.sql.columnar.libpath|Path of the Gluten native library libspark_columnar_plugin.so.|
|spark.driver.extraClassPath, spark.executor.extraClassPath|Paths of the plug-in JAR and dependency JARs. Separate multiple JARs with colons.|
|spark.driverEnv.OMNI_HOME, spark.executorEnv.OMNI_HOME|OmniOperator root directory on the driver and executors.|
|spark.driverEnv.LD_PRELOAD, spark.executorEnv.LD_PRELOAD|Preloads the jemalloc library to optimize memory allocation performance.|

>![Note](public_sys-resources/icon-note.gif) **NOTE:**
>Table 3 lists only the main parameters in the startup commands and configuration files. Other `spark.sql.*` and `spark.gluten.*` parameters are common Spark/Gluten tuning parameters and can be left at their defaults. To adjust them, see the parameter description in the *User Guide*.

### Result Description<a name="result_analysis"></a>

After Q82 is executed, spark-sql outputs `Time taken: xx seconds` at the end of the result. You can also view the execution time on the SQL page of the Spark UI. [**Table 4**](#result_table) lists the comparison results in the reference environment (openEuler 22.03 LTS, Kunpeng 920, 24 executors, and a 40 GB driver; see [Scenario A](#scenario_sparkextension) for the startup parameters).

**Table 4** Q82 execution time comparison<a id="result_table"></a>

|Execution Mode|Q82 Time (s)|Time Reduction|Speedup|
|--|--|--|--|
|Open-source Spark|21.832|-|-|
|SparkExtension|15.066|31.0%|1.45|
|Gluten|9.493|56.5%|2.30|

Results:

- The query results of the three modes are identical (one row, whose first row is `AAAAAAAAEEDAAAAA Dead pages know mo 93.11`), indicating that enabling OmniOperator acceleration does not change the query result.
- After SparkExtension is enabled, the Q82 time decreases from 21.832 seconds to 15.066 seconds, a reduction of about 31.0%. After Gluten is enabled, the time decreases to 9.493 seconds, a reduction of about 56.5%.
- The actual time varies with the cluster specifications, data volume, concurrency, and parameter settings. The data in the table is only used to illustrate the acceleration effect. Use the actual test results in your environment. On a single node with fewer executors, the absolute time is shorter, while the acceleration ratio is similar.

## More Features<a name="more_features"></a>

- For supported operators, expressions, data types, and all tuning parameters, see the [User Guide](user_guide.md).
- For network planning, complete installation steps, and package verification methods, see the [Installation Guide](installation_guide.md).
- For version compatibility and feature changes of each version, see the [Release Notes](release_notes.md).
- For FAQs about installation and usage, see the [FAQs](faq.md).
- For the performance comparison between OmniOperator and Velox on the TPC-DS 10 TB dataset, see the "Performance Data" section in [Project Introduction](../../README_en.md).
- To compile OmniOperator and the Gluten Omni backend from source, or to learn more Gluten startup parameters, see the compile commands and quick start instructions in the [Gluten repository](https://gitcode.com/openeuler/Gluten).

## FAQs<a name="faq"></a>

### How do I verify that OmniOperator acceleration is enabled<a name="faq_q1"></a>

#### Question

After a SQL statement is executed, it is difficult to determine whether native operators are actually used, and you may be concerned that open-source Spark is still executing the query.

#### Answer

Add `EXPLAIN` before the SQL statement to view the execution plan:

- For SparkExtension: if the execution plan contains operators starting with `Omni`, the feature is enabled.
- For Gluten: if the execution plan contains operators starting with `Omni` or ending with `Transformer`, the feature is enabled.

You can also open the Spark UI and view operator names in the execution plan on the SQL page. If no Omni operator is present, check whether `spark.sql.extensions` (SparkExtension) or `spark.plugins` (Gluten) is correctly configured.

### Why are some operators not implemented by Omni<a name="faq_q2"></a>

#### Question

Only some operators in the execution plan start with `Omni`, while other operators are still implemented by open-source Spark.

#### Answer

OmniOperator replaces only the operators and expressions that it supports. When an unsupported scenario is encountered, the operator automatically falls back to the open-source implementation, which is normal. Common causes of fallback include:

- An unsupported operator, expression, or data type is used. For the support scope, see "Supported Operators" and "Supported Expressions" in the [User Guide](user_guide.md).
- In multi-column expression code generation scenarios (for example, 500 columns), the compilation overhead may outweigh the acceleration benefit. In this case, use open-source Spark.
- Some scenarios are explicitly unsupported, such as comparison operators on Boolean data. For details, see "Constraints" in the [Project Introduction](../../README_en.md).

If a key operator such as a large-table join falls back, the row-column conversion overhead may degrade performance. In this case, adjust the SQL statement or data types to avoid fallback.

### What should I do if a SQL query reports insufficient off-heap memory or returns incorrect results<a name="faq_q3"></a>

#### Question

When a large SQL query such as a TPC-DS query is executed, an out-of-off-heap-memory error (for example, `MEM_CAP_EXCEEDED`) is reported, or the query result is inconsistent with that of open-source Spark.

#### Answer

OmniOperator uses off-heap memory to store columnar data. Adjust the configuration in the following order:

1. Increase `spark.memory.offHeap.size`. For TPC-DS 3 TB, at least 20 GB is recommended.
2. Increase `--executor-memory` and `spark.executor.memoryOverhead` appropriately.
3. Adjust `spark.sql.shuffle.partitions` based on the data volume to avoid excessive data in a single partition.
4. If `MEM_CAP_EXCEEDED` is logged but the SQL statement is eventually executed successfully, the result is usually not affected. If the result is abnormal, check whether the off-heap memory is too small.

### Which enabling method should I choose for different Spark versions<a name="faq_q4"></a>

#### Question

Multiple Spark versions exist in the environment, and you are not sure whether to use SparkExtension or Gluten.

#### Answer

- For Spark 3.3.1, 3.4.3, and 3.5.2, Gluten is recommended. However, the server must support the SVE instruction set and run openEuler 22.03 or 24.03.
- For other Spark versions (for example, 3.1.1), use SparkExtension. It supports CentOS 7.9, openEuler 20.03, and openEuler 22.03.
- Run `spark-sql --version` to check the Spark version, and `cat /proc/cpuinfo | grep sve | head -n 1` to check whether SVE is supported.

## Troubleshooting<a name="troubleshooting"></a>

### Case 1: libjemalloc.so.2 Is Not Found When Spark-SQL Starts<a name="trouble_case1"></a>

- **Problem description**: When spark-sql is started, the error `error while loading shared libraries: libjemalloc.so.2: cannot open shared object file: No such file or directory` is reported, and the task cannot start.
- **Impact on the system**: The driver or executor process cannot start, and SQL statements cannot be executed.
- **Possible causes**: `libjemalloc.so.2` is not copied to `/opt/omni-operator/lib`, or the paths configured in `spark.driverEnv.LD_PRELOAD` and `spark.executorEnv.LD_PRELOAD` are incorrect.
- **Troubleshooting approach (optional)**: First check whether the file exists, and then check the path configurations on the driver and executors.
- **Procedure**:
    1. Run the following command to check whether the file exists.

        ```shell
        ls -l /opt/omni-operator/lib/libjemalloc.so.2
        ```

    2. In `spark-defaults-omnioperator.conf`, confirm that the driver path of `LD_PRELOAD` is `/opt/omni-operator/lib/libjemalloc.so.2` and the executor path is `${PWD}/omni/omni-operator/lib/libjemalloc.so.2`.
    3. Restart spark-sql. The error should no longer be reported.

### Case 2: UnsatisfiedLinkError or Native Library Not Found at Startup<a name="trouble_case2"></a>

- **Problem description**: When spark-sql is started or a SQL statement is executed, `java.lang.UnsatisfiedLinkError` is reported, or `no omni in java.library.path` is displayed.
- **Impact on the system**: Native operators cannot be loaded, the task fails to start, or all operators fall back to the open-source implementation.
- **Possible causes**: `--archives` does not distribute the installation package correctly; the JAR names in `spark.driver.extraClassPath` and `spark.executor.extraClassPath` do not match the actual files; in the Gluten scenario, `spark.gluten.sql.columnar.libpath` or `spark.gluten.sql.columnar.executor.libpath` is incorrectly configured.
- **Troubleshooting approach (optional)**: Check the archive distribution, JAR names, and native library paths in that order.
- **Procedure**:
    1. Confirm that the installation package exists on HDFS.

        ```shell
        hdfs dfs -ls /user/root/omni-operator.tar.gz
        ```

    2. Confirm that `/opt/omni-operator/lib` contains the JARs in use and `libspark_columnar_plugin.so` (for Gluten).

        ```shell
        ls /opt/omni-operator/lib | grep -E "gluten-omni-bundle|bindings|libspark_columnar_plugin"
        ```

    3. Replace the JAR names in `extraClassPath` in the configuration file with the actual file names returned by the preceding command, and restart spark-sql.

    Expected result: spark-sql starts successfully. When `EXPLAIN` is executed again, Omni operators appear and `UnsatisfiedLinkError` is no longer reported.

### Case 3: No Omni Operator Appears in the Execution Plan in the Gluten Scenario<a name="trouble_case3"></a>

- **Problem description**: spark-sql is started in Gluten mode, but the `EXPLAIN` output does not contain operators starting with `Omni` or ending with `Transformer`.
- **Impact on the system**: The query is still executed by open-source Spark, and no acceleration is achieved.
- **Possible causes**: The server does not support the SVE instruction set; `spark.gluten.sql.columnar.backend.lib` is not set to `omni`; `spark.plugins` is not configured or the JAR is not loaded.
- **Troubleshooting approach (optional)**: First confirm that the hardware supports SVE, and then check whether the Gluten plug-in configuration takes effect.
- **Procedure**:
    1. Run the following command to check SVE support.

        ```shell
        cat /proc/cpuinfo | grep sve | head -n 1
        ```

        If no output is displayed, SVE is not supported. In this case, use SparkExtension instead.

    2. Confirm that `spark-defaults-omnioperator.conf` contains the following configurations.

        ```text
        spark.plugins org.apache.gluten.GlutenPlugin
        spark.gluten.sql.columnar.backend.lib omni
        spark.gluten.sql.columnar.libpath /opt/omni-operator/lib/libspark_columnar_plugin.so
        ```

    3. Restart spark-sql and run `EXPLAIN SELECT count(*) FROM item;`. The expected output contains operators starting with `Omni` or ending with `Transformer`.

## Change History<a name="revision_history"></a>

|Document Version|Release Date|Change Description|
|--|--|--|
|01|2026-12-30|First official release.|
