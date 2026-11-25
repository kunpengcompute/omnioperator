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


set -euo pipefail

install_bisheng_jdk_17_0_18() {
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    JDK_TAR="bisheng-jdk-17.0.18-b13-linux-aarch64.tar.gz"
    wget --no-check-certificate "https://mirrors.huaweicloud.com/kunpeng/archive/compiler/bisheng_jdk/${JDK_TAR}"

    # Read the top-level directory name from the tar archive.
    set +o pipefail
    JDK_DIR=$(tar -tf "${JDK_TAR}" | head -1 | cut -d/ -f1)
    set -o pipefail

    tar -zxvf "${JDK_TAR}"
    chown -R root:root "${JDK_DIR}"
    rm -f "${JDK_TAR}"

    echo "JDK installed to: /opt/buildtools/${JDK_DIR}"
    sed -i '/JAVA_HOME/d' /etc/profile
    echo "export JAVA_HOME=/opt/buildtools/${JDK_DIR}" >> /etc/profile
    echo 'export PATH=$JAVA_HOME/bin:$PATH' >> /etc/profile
    echo 'export C_INCLUDE_PATH=$JAVA_HOME/include:$JAVA_HOME/include/linux:$C_INCLUDE_PATH' >> /etc/profile
    echo 'export CPLUS_INCLUDE_PATH=$JAVA_HOME/include:$JAVA_HOME/include/linux:$CPLUS_INCLUDE_PATH' >> /etc/profile
    echo 'export LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LIBRARY_PATH' >> /etc/profile
    echo 'export LD_LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH' >> /etc/profile
}

install_bisheng_jdk_8u342(){
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    JDK8_TAR="bisheng-jdk-8u342-linux-aarch64.tar.gz"
    wget -q https://mirrors.huaweicloud.com/kunpeng/archive/compiler/bisheng_jdk/${JDK8_TAR}
    tar -xf ${JDK8_TAR} && rm -rf ${JDK8_TAR} && popd
}

install_cmake_3_28_2() {
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    export CMAKE_VERSION="3.28.2"
    wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-aarch64.tar.gz
    tar -xf cmake-${CMAKE_VERSION}-linux-aarch64.tar.gz && rm -rf cmake-${CMAKE_VERSION}-linux-aarch64.tar.gz  && rm -rf /usr/local/bin/cmake
    cp -r cmake-${CMAKE_VERSION}-linux-aarch64/bin/* /usr/local/bin
    cp -r cmake-${CMAKE_VERSION}-linux-aarch64/share/* /usr/local/share
    popd
}

install_llvm_15_0_4() {
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    wget -q https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-15.0.4.tar.gz
    tar -zxf llvmorg-15.0.4.tar.gz && rm -rf llvmorg-15.0.4.tar.gz && mkdir -p llvm-project-llvmorg-15.0.4/build && pushd llvm-project-llvmorg-15.0.4/build
    # Preserve the original LLVM target option; BPE may have been intended as BPF.
    cmake -G "Unix Makefiles" -DLLVM-TARGETS_TO_BUILD="host;ARM;X86;AArch64;BPE" -DCMAKE_BUILD_TYPE=Release -DLLVM_BUILD_LLVM_DYLIB=true -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_PROJECTS="clang;lld;libunwind;compiler-rt;lldb" -DCMAKE_INSTALL_PREFIX=/usr/local/ ../llvm
    make -j"$BUILD_PARALLELISM" && make install && popd && popd
    rm -rf /opt/buildtools/llvm-project-llvmorg-15.0.4 #####镜像瘦身
    echo 'export LIBRARY_PATH=/usr/local/lib:$LIBRARY_PATH' >> /etc/profile
    echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> /etc/profile
}

install_maven_3_9_9() {
    export MAVEN_VERSION="3.9.9"
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    wget -q https://repo1.maven.org/maven2/org/apache/maven/apache-maven/${MAVEN_VERSION}/apache-maven-${MAVEN_VERSION}-bin.zip
    unzip apache-maven-${MAVEN_VERSION}-bin.zip && rm -rf apache-maven-${MAVEN_VERSION}-bin.zip && popd
    echo "export MAVEN_HOME=/opt/buildtools/apache-maven-${MAVEN_VERSION}" >> /etc/profile
    echo 'export PATH=$MAVEN_HOME/bin:$PATH' >> /etc/profile
}

install_lcov() {
    mkdir -p /opt/buildtools && pushd /opt/buildtools
    wget -q https://github.com/linux-test-project/lcov/releases/download/v1.16/lcov-1.16.tar.gz && tar -xf lcov-1.16.tar.gz && rm -rf lcov-1.16.tar.gz
    pushd lcov-1.16 && make rpms && rpm -ivh lcov-*.noarch.rpm && popd && popd
}

setup_maven() {
    cat > /opt/buildtools/apache-maven-3.9.9/conf/settings.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<settings xmlns="http://maven.apache.org/SETTINGS/1.2.0"
          xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xsi:schemaLocation="http://maven.apache.org/SETTINGS/1.2.0 https://maven.apache.org/xsd/settings-1.2.0.xsd">
    <localRepository>/root/build_cache/.maven_cache</localRepository>
    <mirrors>
        <mirror>
            <id>huaweicloud-maven</id>
            <name>Huawei Cloud Maven Repository</name>
            <url>https://mirrors.huaweicloud.com/repository/maven/</url>
            <!-- mirrorOf=central：仅匹配中央仓库的依赖，优先级最高 -->
            <mirrorOf>central</mirrorOf>
        </mirror>
        <mirror>
            <id>maven-central</id>
            <name>Maven Central Repository</name>
            <url>https://repo1.maven.org/maven2</url>
            <!-- mirrorOf=external:*.googleapis.com：仅匹配谷歌仓库等特殊依赖 -->
            <mirrorOf>external:*.googleapis.com,!huaweicloud-maven</mirrorOf>
        </mirror>
    </mirrors>
    <profiles>
        <profile>
            <id>huaweicloud</id>
            <activation>
                <activeByDefault>true</activeByDefault>
            </activation>
            <repositories>
                <repository>
                    <id>huaweicloud-maven</id>
                    <name>Huawei Cloud Maven Repository</name>
                    <url>https://mirrors.huaweicloud.com/repository/maven/</url>
                    <releases>
                        <enabled>true</enabled>
                    </releases>
                    <snapshots>
                        <enabled>true</enabled>
                    </snapshots>
                </repository>
            </repositories>
            <pluginRepositories>
                <pluginRepository>
                    <id>huaweicloud-maven</id>
                    <name>Huawei Cloud Maven Repository</name>
                    <url>https://mirrors.huaweicloud.com/repository/maven/</url>
                    <releases>
                        <enabled>true</enabled>
                    </releases>
                    <snapshots>
                        <enabled>true</enabled>
                    </snapshots>
                </pluginRepository>
            </pluginRepositories>
        </profile>
    </profiles>
    <activeProfiles>
        <activeProfile>default-profile</activeProfile>
    </activeProfiles>
    <servers>
        <server>
            <id>huaweicloud-maven</id>
            <configuration>
                <connectTimeout>10000</connectTimeout>
                <requestTimeout>120000</requestTimeout>
            </configuration>
        </server>
    </servers>
</settings>
EOF
}
