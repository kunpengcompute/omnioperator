# Preface
This document describes how to compile and deploy the openLooKeng (OLK) and OmniRuntime package for function and performance debugging in ***yellow zone***. The operating system uses ***ubuntu 18.04***.

# Preparing the compilation environment

## Configure HTTP proxy

### Edit file /etc/profile

Add the following information to the file `/etc/profile`

```
export http_proxy=http://username:password@proxy.huawei.com:8080
export https_proxy=http://username:password@proxy.huawei.com:8080
export no_proxy=127.0.0.1,*.huawei.com,localhost,local,*.local,10.*
```

Notices:

- Enter your own domain username and password for the username and password.

- Special characters in the username and password must be escaped. For details, see the following links:

  Common escape character ：<http://3ms.huawei.com/hi/group/4125/wiki_5843646.html>

  All escape character：<http://3ms.huawei.com/hi/group/2901257/wiki_6179442.html>

### Check whether the external network can be pinged

Type the  command `curl www.baidu.com ` on the command line.

Normally, the following information is displayed, indicating that the external network can be pinged successfully.

![1661582746423](http://image.huawei.com/tiny-lts/v1/images/5498ed56051b5e62988d9e2e182b60a3_865x593.png@900-0-90-f.png)

## Configure Ubuntu mirror source

The default mirror source configuration file is  `/etc/apt/sources.list`,which can configure Ubuntu mirror source.However, it is recommended that  add the .list file in the directory `/etc/apt/sources.list.d`.More specifically,create file 00ubuntu.list  in the directory  `/etc/apt/sources.list.d` and add the following content to file 00ubuntu.list.

```
deb http://mirrors.aliyun.com/ubuntu/ bionic main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ bionic-security main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ bionic-updates main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ bionic-proposed main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ bionic-backports main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ bionic main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ bionic-security main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ bionic-updates main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ bionic-proposed main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ bionic-backports main restricted universe multiverse
```

Another solution is to configure the HUAWEI CLOUD source. However, the following error will be reported during subsequent use, which may be caused by the server side.

![1661585348159](http://image.huawei.com/tiny-lts/v1/images/ddba80d5d937f8ac532fec970b66d604_865x399.png@900-0-90-f.png)

After the Ubuntu mirror source configuration, run the following command:

```
apt update
apt upgrade
```

For details about the above configuration, refer to the reference link <http://mirrors.tools.huawei.com/> .

## Install OmniOperatorJit Dependencies

The components required for OmniOperatorJit compilation include cmake, LLVM, googletest, jemalloc, json, huawei securec, JDK, and protobuf. The involved software is as follows:

| Software       | Version |
| -------------- | -------- |
| python         | Python3  |
| gcc/g++        | 7.3.0    |
| zlib           | 1.2.8    |
| autoconf       | 2.69     |
| cmake          | 3.13.4   |
| LLVM           | 12.0.1   |
| gtest          | 1.10.0   |
| jemalloc       | 5.2.1    |
| json           | 3.7.3    |
| huawei securec | 无       |
| JDK            | 1.8      |
| protobuf       | 无       |

### Confirm the Python Version 3

![1661586155228](http://image.huawei.com/tiny-lts/v1/images/538785cfce46f6cac67e0d4600956db0_544x89.png)

Python3 is installed by default.

### Install gcc and g++

#### Install gcc

Run the command `apt-get install gcc `

![1661586482227](http://image.huawei.com/tiny-lts/v1/images/16619fd9c801e744c233b363b18def61_831x131.png)

#### Install g++

Run the command `apt-get install gcc-c++ `

![1661586482227](http://image.huawei.com/tiny-lts/v1/images/4c2cc402beb2f009462560a2f56c1ea7_845x131.png@900-0-90-f.png)

### Install zlib

Run the command `cat /usr/lib64/pkgconfig/zlib.pc ` to check whether zlib has been installed.To install zlib, the installation options are as follows:

- Install zlib via mirror source which means that run the command `apt-get install zlib `

- Install zlib via source code which means that download the source code from http://www.zlib.net/fossils/zlib-1.2.8.tar.gz and run the following command to install zlib

  ```
  tar -xvzf zlib-1.2.8.tar.gz
  cd zlib-1.2.8.tar.gz
  ./configure
  make
  make install
  ```

### Install autoconf

Run the command `apt-get install autoconf `

![1661587943041](http://image.huawei.com/tiny-lts/v1/images/65ede1529b53f9d8c20dd062e1696aec_848x186.png@900-0-90-f.png)

### Install cmake

- Download source code from https://github.com/Kitware/CMake/archive/refs/tags/v3.13.4.tar.gz

- Run the following command to install cmake via  the source code

  ```
  tar zxvf CMake-3.13.4.tar.gz
  cd CMake-3.13.4
  ./bootstrap
  gmake
  gmake install
  ```


### Install LLVM

- Download source code from <https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-12.0.1.tar.gz>  

- Run the following command to install LLVM via  the source code

  ```
  tar zxvf llvm-project-llvmorg-12.0.1.tar.gz
  cd llvm-project-llvmorg-12.0.1
  mkdir build
  cd build
  cmake -DCMAKE_INSTALL_PREFIX=/opt/omni-operator/llvm12 -DCMAKE_BUILD_TYPE=Release -DLLVM_BUILD_LLVM_DYLIB=true -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi" -G "Unix Makefiles" ../llvm
  make -j4
  make install
  ```

  Notice:the path `/opt/omni-operator/llvm12 `can be customized.

### Install gtest

- Download source code from  <https://github.com/google/googletest/archive/refs/tags/release-1.10.0.tar.gz> 

- Run the following command to install gtest via  the source code

  ```
  tar zxvf googletest-release-1.10.0.tar.gz
  cd googletest-release-1.10.0
  cmake CMakeLists.txt
  make
  make install
  ```

### Install jemalloc

- Download source code from <https://github.com/jemalloc/jemalloc/archive/refs/tags/5.2.1.tar.gz> 

- Run the following command to install jemalloc via  the source code

  ```
  tar zxvf jemalloc-5.2.1.tar.gz
  cd jemalloc-5.2.1
  ./autogen.sh --disable-initial-exec-tls
  make -j2
  sudo make install
  ```

### Install nlohmann json

The json version of OmniOperator requires at least 3.7.3. If the GCC version is 10.., choose a later version of json such as 3.9.1. Otherwise, it will result in compilation failure.If the GCC version is 7.3.0,the json version 3.7.3 is recommended.

- Download source code from <https://github.com/nlohmann/json/archive/refs/tags/v3.9.1.tar.gz> <https://github.com/nlohmann/json/archive/refs/tags/v3.7.3.tar.gz> 

- Run the following command to install jemalloc via  the source code

  ```
  tar zxvf json-3.9.1.tar.gz
  cd json-3.9.1
  mkdir build
  cd build/
  cmake ..
  make
  make install
  ```
Notice:The installation procedures for versions 3.9.1 and 3.7.3 are the same.

### Install rapid json
The RapidJSON dependency can be installed by placing the RapidJSON header files under /usr/local/include. RapidJSON is a header-only C++ library, so it does not need to be compiled.

* Download source code from https://github.com/Tencent/rapidjson￼
* Run the following command to add RapidJSON to /usr/local/include
```bash
git clone https://github.com/Tencent/rapidjson.git
cd rapidjson
sudo cp -r include/rapidjson /usr/local/include/
```

### Install libboundscheck

- Download source code from <https://gitee.com/openeuler/libboundscheck> 

- Run the following command to install jemalloc via  the source code

  ```
  git clone https://gitee.com/openeuler/libboundscheck.git
  cd libboundscheck
  make CC=gcc 
  ```

### Install JDK

Run the command `apt-get install java-8-openjdk `

### Install  protobuf

To install protobuf, the installation options are as follows:

- Install protobuf via mirror source which means that run the command `apt-get install protobuf-compiler`

- Install protobuf via source code which means that run the command `git clone [git@github.com](mailto:git@github.com):protocolbuffers/protobuf.git ` to download the source code and run the following command to install protobuf

  ```
  apt-get install autoconf automake libtool curl make g++ unzip
  ./autogen.sh 
  ./configure 
  make
  make check
  sudo make install
  sudo ldconfig
  ```

If the version number is displayed after run the command  `protoc --version` , the installation of protobuf is successful.

### Configure environment variables

Run the command `vi /etc/profile `,and add the following content to the file:

- Configure LLVM

  To compile OmniOperatorJit,  the LLVM configuration is as follows:

  ```
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/opt/omni-operator/llvm12/lib
  export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/opt/omni-operator/llvm12/lib
  export PATH=${PATH}:/opt/omni-operator/llvm12/bin
  export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/omni-operator/llvm12/include
  export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/opt/omni-operator/llvm12/include
  ```

  Adjust the path that the variables point to based on the path of the LLVM compiled previously.

- Configure huawei securec

  To compile OmniOperatorJit,  the huawei securec configuration is as follows:

  ```
  export LD_LIBRARY_PATH(add the path) ":/opt/omni-operator/software/libboundscheck/lib"
  export LIBRARY_PATH(add the path) ":/opt/omni-operator/software/libboundscheck/lib"
  export C_INCLUDE_PATH(add the path) ":/opt/omni-operator/software"
  export CPLUS_INCLUDE_PATH(add the path) ":/opt/omni-operator/software"
  ```

  libboundscheck is stored in the directory `/opt/omni-operator/software` . It is acceptable to place libboundscheck in a proper directory and specify the defined directory in the configuration file.

- Configure JDK

  Run the command `find / -name "jni.h" ` to find the JDK installation location and configure it in `/etc/profile`

  ![1661592130232](http://image.huawei.com/tiny-lts/v1/images/260d2c72c5a7446a2adc40818f93281c_628x47.png)

  ![1661592155915](http://image.huawei.com/tiny-lts/v1/images/079e4d3db80efc911d9c6caf1eb9a7dd_865x130.png)

  Finally,run the command `source /etc/profile` to update environment variables.

  In the section 2.3.6 LLVM Installation, the path of installation of  LLVM is directory `/opt/omni-operator/llvm12`

  ```
  cd /opt/omni-operator/llvm12
  ln –s clang++ clang++-12
  ```

## Compile OmniOperatorJit

code repository

- openLooKeng Extension code repository: <https://github.com/kunpengcompute/boostkit-bigdata/tree/main/omnioperator/omniop-openlookeng-extension> 
- OmniOperatorJIT code repository:<https://codehub-y.huawei.com/Kunpeng-Computing-DC/BigData/OmniOperatorJIT> 
- openLooKeng code repository:<https://gitee.com/openlookeng/hetu-core> 

Clone code from the preceding repositories to the local host. The compilation sequence is OmniOperator Jit Core, OmniOperator Jit bindings, omniop-openlookeng-extension, and openLooKeng.

 Notice: the openLooKeng branch must be set to 1.6 or later.

### Compile OmniOperatorJit Core

- Run command `cd /OmniOperatorJIT/core/build/ `to go to the directory

  - Run the different compilation commands based on different options, as follows:

    ```
    RELEASE:
    sh build.sh release [--disable-cpuchecker] [--enable-dt]
  
    DEBUG:
    sh build.sh debug [op/vec/llvm/all] [--disable-cpuchecker] [--enable-dt]
  
    TRACE:
    sh build.sh trace [op/vec/llvm/all] [--disable-cpuchecker] [--enable-dt]
  
    COVERAGE:
    sh build.sh coverage [op/vec/llvm/all] [--disable-cpuchecker] [--enable-dt]
    ```

    Notice: run the following command to download the jar package before compilation:

    ```
    wget https://szxy1.artifactory.cd-cloud-artifact.tools.huawei.com/artifactory/sz-maven-public/com/huawei/devtest/devtestcov-maven-plugin/2.1.1/devtestcov-maven-plugin-2.1.1.jar --proxy=off --no-check-certificate
  
    wget https://szxy1.artifactory.cd-cloud-artifact.tools.huawei.com/artifactory/sz-maven-public/com/huawei/devtest/devtestcov-maven-plugin/2.1.1/devtestcov-maven-plugin-2.1.1.pom --proxy=off --no-check-certificate
  
    mvn install:install-file -Dfile=devtestcov-maven-plugin-2.1.1.jar -DpomFile=devtestcov-maven-plugin-2.1.1.pom
    ```
  
    ```
    wget https://cmc.centralrepo.rnd.huawei.com/artifactory/product_maven/com/huawei/bepcloud/rebuild-maven-plugin/2.1.T1.SPC2/rebuild-maven-plugin-2.1.T1.SPC2.jar --proxy=off --no-check-certificate
    
    wget https://cmc.centralrepo.rnd.huawei.com/artifactory/product_maven/com/huawei/bepcloud/rebuild-maven-plugin/2.1.T1.SPC2/rebuild-maven-plugin-2.1.T1.SPC2.pom --proxy=off --no-check-certificate
    
    mvn install:install-file -Dfile=rebuild-maven-plugin-2.1.T1.SPC2.jar -DpomFile=rebuild-maven-plugin-2.1.T1.SPC2.pom
    ```

### Compile OmniOperatorJit bindings

- Run command `cd /OmniOperatorJIT/bindings/java `to go to the directory
- Run command `mvn clean install -DskipTests -T 1C ` to compile code,and the compiled target jar package is stored in the directory `/OmniOperatorJIT/bindings/java/target/ `

### Compile omni-openlookeng-extension 

- Run command `cd /hetu-core ` to go to the directory and choose the branch-1.6
- Run command `mvn clean install -DskipTests -T 1C ` to compile code and the compiled target jar package is stored in the directory `/hetu-core/hetu-server/target/ `.In addition, it is acceptable to download it from the official website of openLooKeng.

## Install openLooKeng with OmniOperatorJit

### Prepare documents

In this example,` /opt/omni-operator` is used as the root directory for installing openLooKeng.

Obtain the following packages and files from the compiled target files:

hetu-server-*.*.*-SNAPSHOT.tar.gz

omni-openLooKeng-adapter-*.*.*-SNAPSHOT.jar

boostkit-omniop-bindings-1.0.0-aarch64.jar

*. *. * indicates the version number, for example, 1.6.1.

### Deploy the OpenLooKeng Engine

- Deploy the openLooKeng engine on the node. For details, see the openLooKeng deployment guide:https://docs.openlookeng.io/zh/docs/docs/installation/deployment.html.

- After the openLooKeng is deployed, locate the deployment directory of the openLooKeng engine (for example, `/opt/hetu-server-1.6.1`) and place the file boostkit-omniop-openlookeng-1.6.1-1.0.0-aarch64.jar to the directory `/opt/omni-operator` .

- Configure openLooKengExtension

  Add the following content to file hetu-server-1.6.1/etc/config.properties

  ```
  extension_execution_planner_enabled=false
  extension_execution_planner_jar_path=file:///opt/omni-operator/boostkit-omniop-openlookeng-1.6.1-1.0.0-aarch64.jar
  extension_execution_planner_class_path=nova.hetu.olk.OmniLocalExecutionPlanner
  ```

- Copy boostkit-omniop-bindings-1.0.0-aarch64.jar to the lib directory of hetu-server-1.6.1.

- Start hetu-server 

  ```
  export LD_LIBRARY_PATH=/opt/omni-operator/lib
  export OMNI_HOME=/opt/omni-operator
  /opt/hetu-server-1.6.1/bin/launcher start
  ```

# Common issues
1. ***Error***: fatal, unable to access 'https://github.com/jemalloc/jemalloc.git/': server certificate verification failed. CAfile: /etc/ssl/certs/ca-certificates.crt CRLfile: none. <br/>
      ***Solution***: `export GIT_SSL_NO_VERIFY=1`
2. ***Error***: fatal, unable to access 'https://codehub-dg-y.huawei.com/data-app-lab/omni-runtime.git/': Received HTTP code 504 from proxy after CONNECT. <br/>
      ***Solution***: `unset http_proxy; unset https_proxy`
3. ***Tip***: If your username or password has some special characters, please add `'\'` before them when configuring proxy.



