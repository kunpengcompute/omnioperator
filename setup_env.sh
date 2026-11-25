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

# ============================================================================
# OmniOperator_2129 环境配置脚本
# 适配 Tier 1 预构建镜像: openeuler22.03_lts_sp3:arm64_003
# 适配架构: aarch64 (Kunpeng 920 + SVE)
#
# 用法:
#   bash setup_env.sh [仓库根目录绝对路径]
#   默认仓库路径: /home/workspace/omnioperator_2129-verify
#
# 前置条件:
#   - 容器已启动并挂载了 /home/workspace
#   - /home/workspace/libboundscheck 存在
#   - /home/workspace/json 存在
# ============================================================================

set -euo pipefail

# 默认仓库路径
REPO_DIR="${1:-/home/workspace/omnioperator_2129-verify}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ============================================================================
# 1. 基础环境变量 (参考 CI 脚本 OmniOperatorJIT_compile.sh config_env)
# ============================================================================
log_info "=== 1. 设置基础环境变量 ==="

# JAVA_HOME (bisheng-jdk 1.8.0_342)
JAVA_HOME_PATH="/opt/buildtools/bisheng-jdk1.8.0_342"
if [ -d "$JAVA_HOME_PATH" ]; then
    export JAVA_HOME="$JAVA_HOME_PATH"
    export JRE_HOME="$JAVA_HOME_PATH/jre"
    export CLASSPATH="${JRE_HOME}/lib:${CLASSPATH:-}"
    log_info "JAVA_HOME = $JAVA_HOME"
else
    log_error "JAVA_HOME 路径不存在: $JAVA_HOME_PATH"
    exit 1
fi

# OMNI_HOME (安装目标路径, CI 用 $(pwd), 这里用仓库根目录)
export OMNI_HOME="${REPO_DIR}"
mkdir -p "${OMNI_HOME}/lib/include"
log_info "OMNI_HOME = $OMNI_HOME"

# 编译线程数 (使用全部核数)
export OMNI_COMPILER_THREAD_COUNT=$(nproc)
log_info "OMNI_COMPILER_THREAD_COUNT = $OMNI_COMPILER_THREAD_COUNT (nproc)"

# Protobuf
export PROTOBUF_HOME="/opt/buildtools/Protobuf-3.21.9"
export Protobuf_ROOT="$PROTOBUF_HOME"
export Protobuf_PROTOC_EXECUTABLE="$PROTOBUF_HOME/bin/protoc"
log_info "PROTOBUF_HOME = $PROTOBUF_HOME"

# LLVM
export LLVM_HOME="/opt/buildtools/LLVM-15.0.4"
log_info "LLVM_HOME = $LLVM_HOME"

# Maven
export MAVEN_HOME="/opt/buildtools/apache-maven/apache-maven-3.9.9"
log_info "MAVEN_HOME = $MAVEN_HOME"

# CMake
export CMAKE_ROOT="/opt/buildtools/cmake-3.28.2-linux-aarch64/share"
log_info "CMAKE_ROOT = $CMAKE_ROOT"

# FMT / FOLLY
export FMT_HOME="/usr/local"
export FOLLY_HOME="/usr/local"

# CMAKE_PREFIX_PATH (CMake 查找 protobuf 等)
export CMAKE_PREFIX_PATH="$PROTOBUF_HOME"
log_info "CMAKE_PREFIX_PATH = $CMAKE_PREFIX_PATH"

# OS 版本信息 (CI 用于打包, 这里记录)
os_version=$(grep PRETTY_NAME /etc/os-release | cut -d= -f2 | sed -E 's/"([A-Za-z0-9]+) ([0-9]+\.[0-9]+).*/\1\2/')
export OS_type="${os_version}"
log_info "OS_type = $OS_type"

# liborc.so (CI 复制到 /usr/local/lib/)
if [ -f "/opt/Dependencies_${os_version}_Adaptor/liborc.so" ]; then
    sudo cp -f "/opt/Dependencies_${os_version}_Adaptor/liborc.so" /usr/local/lib/ 2>/dev/null || \
        cp -f "/opt/Dependencies_${os_version}_Adaptor/liborc.so" /usr/local/lib/
    log_info "liborc.so -> /usr/local/lib/"
fi

# ============================================================================
# 2. 修复 /opt/lib (可能残留为文件)
# ============================================================================
log_info "=== 2. 修复 /opt/lib ==="

if [ -f "/opt/lib" ]; then
    rm -f /opt/lib
    log_warn "/opt/lib 是文件而非目录，已删除"
fi
if [ ! -d "/opt/lib" ]; then
    mkdir -p /opt/lib
    log_info "创建 /opt/lib 目录"
else
    log_info "/opt/lib 目录已存在"
fi

# ============================================================================
# 3. LLVM 软链接 (CMakeLists.txt 硬编码 /usr/lib/llvm-15/include)
# ============================================================================
log_info "=== 3. 创建 LLVM 软链接 ==="

LLVM_SRC="/opt/buildtools/LLVM-15.0.4"
LLVM_DST="/usr/lib/llvm-15"

if [ -d "$LLVM_SRC" ]; then
    # 如果是文件则先删除
    if [ -f "$LLVM_DST" ]; then
        rm -f "$LLVM_DST"
    fi
    if [ ! -e "$LLVM_DST" ]; then
        mkdir -p /usr/lib
        ln -sf "$LLVM_SRC" "$LLVM_DST"
        log_info "软链接创建: $LLVM_DST -> $LLVM_SRC"
    else
        log_info "软链接已存在: $LLVM_DST"
    fi

    # clang++-15 软链接
    ln -sf "$LLVM_SRC/bin/clang++" /usr/bin/clang++-15
    ln -sf "$LLVM_SRC/bin/clang-15" /usr/bin/clang-15
    ln -sf "$LLVM_SRC/lib/libLLVM-15.so" /usr/lib64/libLLVM-15.so
    log_info "clang++-15 / clang-15 / libLLVM-15.so 软链接已创建"
else
    log_error "LLVM 源路径不存在: $LLVM_SRC"
    exit 1
fi

# ============================================================================
# 4. 动态库复制到系统路径
# ============================================================================
log_info "=== 4. 复制动态库到系统路径 ==="

mkdir -p /usr/local/lib64

# 4.1 libboundscheck.so (先编译再复制)
log_info "--- 4.1 编译并安装 libboundscheck ---"
BOUNDSCHECK_DIR="/home/workspace/libboundscheck"
if [ -d "$BOUNDSCHECK_DIR" ]; then
    cd "$BOUNDSCHECK_DIR"
    make CC=gcc 2>&1 | tail -1
    cp -f lib/libboundscheck.so /usr/local/lib64/
    log_info "libboundscheck.so -> /usr/local/lib64/"

    # 创建头文件目录结构 (代码 #include <libboundscheck/include/securec.h>)
    mkdir -p /usr/local/include/libboundscheck/include
    cp -f include/* /usr/local/include/libboundscheck/include/
    log_info "libboundscheck 头文件 -> /usr/local/include/libboundscheck/include/"
    cd "$REPO_DIR"
else
    log_error "libboundscheck 目录不存在: $BOUNDSCHECK_DIR"
    exit 1
fi

# 4.2 libLLVM-15.so
log_info "--- 4.2 复制 LLVM 动态库 ---"
if [ -d "$LLVM_SRC/lib" ]; then
    for lib in libLLVM-15.so libLLVM-15.0.4.so libLLVM.so; do
        if [ -f "$LLVM_SRC/lib/$lib" ]; then
            cp -f "$LLVM_SRC/lib/$lib" /usr/local/lib64/
            log_info "复制 $lib -> /usr/local/lib64/"
        fi
    done
fi

# 4.3 libjemalloc.so.2
log_info "--- 4.3 复制 jemalloc ---"
JEMALLOC_FOUND=0
for jemalloc_path in \
    /usr/local/lib/libjemalloc.so.2 \
    /opt/Dependencies_openEuler22.03_OmniStream/libjemalloc.so.2 \
    /opt/Dependencies_openEuler22.03_Adaptor/libjemalloc.so.2; do
    if [ -f "$jemalloc_path" ]; then
        cp -f "$jemalloc_path" /usr/local/lib64/
        ln -sf /usr/local/lib64/libjemalloc.so.2 /usr/lib64/libjemalloc.so
        log_info "复制 libjemalloc ($jemalloc_path) -> /usr/local/lib64/"
        JEMALLOC_FOUND=1
        break
    fi
done
if [ $JEMALLOC_FOUND -eq 0 ]; then
    log_warn "libjemalloc.so.2 未找到"
fi

# 4.4 创建库软链接供链接器查找 (-lboundscheck -ljemalloc -lgtest -lgmock)
log_info "--- 4.4 创建库软链接 ---"
ln -sf /usr/local/lib64/libboundscheck.so /usr/lib64/libboundscheck.so 2>/dev/null
ln -sf /usr/local/lib64/libre2.so /usr/lib64/libre2.so 2>/dev/null
if [ $JEMALLOC_FOUND -eq 1 ]; then
    ln -sf /usr/local/lib64/libjemalloc.so.2 /usr/lib64/libjemalloc.so.2 2>/dev/null
fi

# ============================================================================
# 5. GTest/GMock 版本匹配修复
# ============================================================================
log_info "=== 5. 修复 GTest/GMock 版本匹配 ==="

# 问题: 镜像中 gtest.a 是旧版 (GetCurrentOsStackTraceExceptTop(int) 单参数)
#       gmock.a 需要新版 (GetCurrentOsStackTraceExceptTop(UnitTest*, int) 双参数)
# 解决: 查找匹配版本的 gtest+gmock，优先使用 omnistatestore 编译的匹配版本

GTEST_FIXED=0

# 尝试从 omnistatestore-verify 获取匹配的 gtest+gmock
OMNISTATESTORE_GTEST="/home/workspace/omnistatestore-verify/dist/3rdparty/googletest/lib"
OMNISTATESTORE_INCLUDE="/home/workspace/omnistatestore-verify/dist/3rdparty/googletest/include"
if [ -d "$OMNISTATESTORE_GTEST" ] && \
   [ -f "$OMNISTATESTORE_GTEST/libgtest.a" ] && \
   [ -f "$OMNISTATESTORE_GTEST/libgmock.a" ]; then
    # 复制库文件
    cp -f "$OMNISTATESTORE_GTEST/libgtest.a" /usr/local/lib64/libgtest.a
    cp -f "$OMNISTATESTORE_GTEST/libgtest_main.a" /usr/local/lib64/libgtest_main.a
    cp -f "$OMNISTATESTORE_GTEST/libgmock.a" /usr/lib64/libgmock.a
    cp -f "$OMNISTATESTORE_GTEST/libgmock_main.a" /usr/lib64/libgmock_main.a
    # 同步复制对应的头文件 (确保头文件和库版本一致, 避免 lld 链接器 ABI 不匹配)
    if [ -d "$OMNISTATESTORE_INCLUDE" ]; then
        rm -rf /usr/local/include/gtest /usr/local/include/gmock
        cp -rf "$OMNISTATESTORE_INCLUDE/gtest" /usr/local/include/gtest
        cp -rf "$OMNISTATESTORE_INCLUDE/gmock" /usr/local/include/gmock 2>/dev/null || true
        log_info "gtest/gmock 头文件已同步到 /usr/local/include/"
    fi
    log_info "从 omnistatestore-verify 获取匹配的 gtest+gmock (库+头文件)"
    GTEST_FIXED=1
fi

if [ $GTEST_FIXED -eq 0 ]; then
    # 检查镜像自带的 gtest/gmock 是否匹配
    GTEST_SYM=$(nm /usr/local/lib64/libgtest.a 2>/dev/null | grep "GetCurrentOsStackTraceExceptTop" | head -1)
    GMOCK_SYM=$(nm /usr/lib64/libgmock.a 2>/dev/null | grep "GetCurrentOsStackTraceExceptTop" | head -1)
    if [ -n "$GTEST_SYM" ] && [ -n "$GMOCK_SYM" ]; then
        log_warn "gtest/gmock 版本可能不匹配，lld 链接器可能失败"
        log_warn "gtest: $GTEST_SYM"
        log_warn "gmock: $GMOCK_SYM"
        log_warn "如遇 lld 链接错误，请从 omnistatestore 或 xucg 获取匹配版本"
    else
        log_warn "未找到 gmock，可能影响链接"
    fi
fi

# ============================================================================
# 6. lld 链接器设置 (加速 omtest 链接)
# ============================================================================
# lld 默认启用, 配合 LIBRARY_PATH (步骤8) 和 LDFLAGS 中的 benchmark 路径解决链接问题
# 如遇 lld 链接错误, 可设置 ENABLE_LLD=0 回退到 GNU ld
log_info "=== 6. lld 链接器设置 (默认启用) ==="

LLD_PATH="/opt/buildtools/LLVM-15.0.4/bin/ld.lld"
if [ -f "$LLD_PATH" ] && [ "${ENABLE_LLD:-1}" = "1" ]; then
    export PATH="/opt/buildtools/LLVM-15.0.4/bin:${PATH}"
    export LDFLAGS="-fuse-ld=lld -L/usr/local/lib64 -L/usr/lib64 -L${REPO_DIR}/open_source/benchmark/build/src"
    log_info "lld 链接器已启用: $LLD_PATH (ENABLE_LLD=${ENABLE_LLD:-1})"
    log_info "LDFLAGS = $LDFLAGS"
    log_info "如遇 lld 链接错误, 可: export ENABLE_LLD=0 && bash setup_env.sh 回退到 GNU ld"
else
    log_info "使用默认 GNU ld (lld 已禁用, ENABLE_LLD=${ENABLE_LLD:-1})"
    log_info "如需启用 lld 加速链接, 请: export ENABLE_LLD=1 && bash setup_env.sh"
fi

# ============================================================================
# 7. C_INCLUDE_PATH / CPLUS_INCLUDE_PATH (参考 CI config_env)
# ============================================================================
log_info "=== 7. 设置 C_INCLUDE_PATH / CPLUS_INCLUDE_PATH ==="

# CI 的 include 路径 (包含 rapidjson, protobuf, llvm, java, orc, Adaptor 等)
export C_INCLUDE_PATH="${OMNI_HOME}/rapidjson/include:${PROTOBUF_HOME}/include:${LLVM_HOME}/include:${JAVA_HOME}/include:${JAVA_HOME}/include:${OMNI_HOME}/lib/include:${OMNI_HOME}:/usr/local/include/orc/:/opt/Adaptor/include:/opt/Adaptor/lib/include:/usr/local/include:${C_INCLUDE_PATH:-}"
export CPLUS_INCLUDE_PATH="${OMNI_HOME}/rapidjson/include:${PROTOBUF_HOME}/include:${LLVM_HOME}/include:${JAVA_HOME}/include:${JAVA_HOME}/include:${OMNI_HOME}/lib/include:${OMNI_HOME}:/usr/local/include/orc/:/opt/Adaptor/include:/opt/Adaptor/lib/include:/usr/local/include:${CPLUS_INCLUDE_PATH:-}"
log_info "C_INCLUDE_PATH 已设置 (参考 CI config_env)"
log_info "CPLUS_INCLUDE_PATH 已设置 (参考 CI config_env)"

# 如果 open_source 目录不存在，预先创建并复制依赖源码
if [ ! -d "${REPO_DIR}/open_source/libboundscheck" ]; then
    mkdir -p "${REPO_DIR}/open_source"
    cp -r /home/workspace/libboundscheck "${REPO_DIR}/open_source/" 2>/dev/null || true
    cp -r /home/workspace/json "${REPO_DIR}/open_source/" 2>/dev/null || true
    log_info "已预创建 open_source 目录并复制 libboundscheck/json"
fi

# ============================================================================
# 8. LD_LIBRARY_PATH / LIBRARY_PATH (参考 CI config_env)
# ============================================================================
log_info "=== 8. 设置 LD_LIBRARY_PATH / LIBRARY_PATH ==="

# CI 的 LD_LIBRARY_PATH (运行时库路径)
export LD_LIBRARY_PATH="${PROTOBUF_HOME}/lib:${LLVM_HOME}/lib:${OMNI_HOME}/lib:/opt/Adaptor/lib:/usr/local/lib:/usr/local/lib64:/opt/lib:${LD_LIBRARY_PATH:-}"
log_info "LD_LIBRARY_PATH 已设置 (参考 CI config_env)"

# CI 的 LIBRARY_PATH (链接时库路径, 关键! benchmark 链接需要)
export LIBRARY_PATH="${PROTOBUF_HOME}/lib:${LLVM_HOME}/lib:${OMNI_HOME}/lib:/opt/Adaptor/lib:/usr/local/lib:/usr/local/lib64:${LIBRARY_PATH:-}"
log_info "LIBRARY_PATH 已设置 (参考 CI config_env, 解决 benchmark 链接问题)"

# ============================================================================
# 9. ulimit 设置 (避免 cc1plus 栈溢出)
# ============================================================================
log_info "=== 9. 设置 ulimit ==="

ulimit -s unlimited
log_info "ulimit -s = $(ulimit -s)"

# ============================================================================
# 10. PATH 更新 (参考 CI config_env)
# ============================================================================
log_info "=== 10. 更新 PATH ==="

# CI 的 PATH (包含 cmake, LLVM, protobuf, maven, java, ~/.local/bin)
export PATH="$HOME/.local/bin:/opt/buildtools/cmake-3.28.2-linux-aarch64/bin:${LLVM_HOME}/bin:${PROTOBUF_HOME}/bin:${MAVEN_HOME}/bin:${JAVA_HOME}/bin:${JAVA_HOME}/jre/bin:${PATH}"
log_info "PATH 已更新 (参考 CI config_env)"

# ============================================================================
# 11. 持久化环境变量到 /etc/profile.d/
# ============================================================================
log_info "=== 11. 持久化环境变量到 /etc/profile.d/ ==="

cat > /etc/profile.d/omnioperator_env.sh << 'ENVEOF'
#!/bin/bash
# OmniOperator 环境变量 (由 setup_env.sh 生成, 参考 CI config_env)

export JAVA_HOME="/opt/buildtools/bisheng-jdk1.8.0_342"
export JRE_HOME="$JAVA_HOME/jre"
export OMNI_HOME="__REPO_DIR__"
export OMNI_COMPILER_THREAD_COUNT=$(nproc)
export PROTOBUF_HOME="/opt/buildtools/Protobuf-3.21.9"
export Protobuf_ROOT="$PROTOBUF_HOME"
export Protobuf_PROTOC_EXECUTABLE="$PROTOBUF_HOME/bin/protoc"
export LLVM_HOME="/opt/buildtools/LLVM-15.0.4"
export MAVEN_HOME="/opt/buildtools/apache-maven/apache-maven-3.9.9"
export CMAKE_ROOT="/opt/buildtools/cmake-3.28.2-linux-aarch64/share"
export FMT_HOME="/usr/local"
export FOLLY_HOME="/usr/local"
export CMAKE_PREFIX_PATH="$PROTOBUF_HOME"

export C_INCLUDE_PATH="${OMNI_HOME}/rapidjson/include:${PROTOBUF_HOME}/include:${LLVM_HOME}/include:${JAVA_HOME}/include:${OMNI_HOME}/lib/include:${OMNI_HOME}:/usr/local/include/orc/:/opt/Adaptor/include:/opt/Adaptor/lib/include:/usr/local/include:${C_INCLUDE_PATH}"
export CPLUS_INCLUDE_PATH="${OMNI_HOME}/rapidjson/include:${PROTOBUF_HOME}/include:${LLVM_HOME}/include:${JAVA_HOME}/include:${OMNI_HOME}/lib/include:${OMNI_HOME}:/usr/local/include/orc/:/opt/Adaptor/include:/opt/Adaptor/lib/include:/usr/local/include:${CPLUS_INCLUDE_PATH}"
export LD_LIBRARY_PATH="${PROTOBUF_HOME}/lib:${LLVM_HOME}/lib:${OMNI_HOME}/lib:/opt/Adaptor/lib:/usr/local/lib:/usr/local/lib64:/opt/lib:${LD_LIBRARY_PATH}"
export LIBRARY_PATH="${PROTOBUF_HOME}/lib:${LLVM_HOME}/lib:${OMNI_HOME}/lib:/opt/Adaptor/lib:/usr/local/lib:/usr/local/lib64:${LIBRARY_PATH}"
export PATH="$HOME/.local/bin:/opt/buildtools/cmake-3.28.2-linux-aarch64/bin:${LLVM_HOME}/bin:${PROTOBUF_HOME}/bin:${MAVEN_HOME}/bin:${JAVA_HOME}/bin:${JAVA_HOME}/jre/bin:${PATH}"

# lld 链接器 (默认启用, 配合 LIBRARY_PATH 解决 benchmark 链接)
export ENABLE_LLD="${ENABLE_LLD:-1}"
if [ "${ENABLE_LLD}" = "1" ] && [ -f "/opt/buildtools/LLVM-15.0.4/bin/ld.lld" ]; then
    export PATH="/opt/buildtools/LLVM-15.0.4/bin:${PATH}"
    export LDFLAGS="-fuse-ld=lld -L/usr/local/lib64 -L/usr/lib64 -L${OMNI_HOME}/open_source/benchmark/build/src"
fi
ulimit -s unlimited 2>/dev/null
ENVEOF

# 替换 CPATH 中的仓库路径
sed -i "s|__REPO_DIR__|${REPO_DIR}|g" /etc/profile.d/omnioperator_env.sh
log_info "环境变量已写入 /etc/profile.d/omnioperator_env.sh"

# ============================================================================
# 12. 前置检查
# ============================================================================
log_info "=== 12. 前置检查 ==="

# 检查同级依赖目录
DEP_PARENT=$(dirname "$REPO_DIR")
for dep in libboundscheck json; do
    if [ -d "${DEP_PARENT}/${dep}" ]; then
        log_info "✓ ${DEP_PARENT}/${dep} 存在"
    else
        log_error "✗ ${DEP_PARENT}/${dep} 不存在"
        log_error "build.sh package 需要 ../${dep} 同级目录"
        exit 1
    fi
done

# 检查编译器
if command -v gcc &>/dev/null; then
    log_info "✓ gcc $(gcc --version | head -1)"
else
    log_error "✗ gcc 未安装"
    exit 1
fi

if command -v cmake &>/dev/null; then
    log_info "✓ cmake $(cmake --version | head -1)"
else
    log_error "✗ cmake 未安装"
    exit 1
fi

# 检查 LLVM
if [ -f "/usr/lib/llvm-15/include/llvm/ADT/APInt.h" ]; then
    log_info "✓ LLVM 15 头文件可访问"
else
    log_error "✗ LLVM 15 头文件不可访问"
    exit 1
fi

if command -v clang++-15 &>/dev/null; then
    log_info "✓ clang++-15 $(clang++-15 --version 2>&1 | head -1)"
else
    log_error "✗ clang++-15 不可用"
    exit 1
fi

# 检查 lld
if command -v ld.lld &>/dev/null; then
    log_info "✓ lld $(ld.lld --version 2>&1 | head -1)"
else
    log_warn "✗ lld 不可用，将使用默认 GNU ld"
fi

# 检查动态库
for lib in libboundscheck.so libLLVM-15.so; do
    if ldconfig -p | grep -q "$lib"; then
        log_info "✓ $lib 在 ldconfig 缓存中"
    else
        log_warn "✗ $lib 不在 ldconfig 缓存中"
    fi
done

# 检查 gtest/gmock
if [ -f "/usr/local/lib64/libgtest.a" ]; then
    log_info "✓ libgtest.a 存在"
else
    log_warn "✗ libgtest.a 不存在"
fi
if [ -f "/usr/lib64/libgmock.a" ]; then
    log_info "✓ libgmock.a 存在"
else
    log_warn "✗ libgmock.a 不存在"
fi

# 检查 /opt/lib 是目录
if [ -d "/opt/lib" ]; then
    log_info "✓ /opt/lib 是目录"
else
    log_error "✗ /opt/lib 不是目录"
    exit 1
fi

# ============================================================================
# 13. 构建命令提示
# ============================================================================
echo ""
echo "=========================================="
echo "  环境配置完成"
echo "=========================================="
echo ""
echo "构建命令 (在仓库根目录执行):"
echo "  cd $REPO_DIR"
echo "  sh build_scripts/build.sh coverage:java    # 编译(C++库+omtest)"
echo ""
echo "执行 UT:"
echo "  ./build/core/test/omtest                   # 运行12573个测试"
echo ""
echo "注意:"
echo "  - sh build_scripts/build.sh 直接调用子脚本, 不会被 env_check.sh 的 OMNI_COMPILER_THREAD_COUNT=8 覆盖"
echo "  - lld 链接器已启用 (LDFLAGS=-fuse-ld=lld), 加速 omtest 链接"
echo "  - 每次重新构建前需清理: rm -rf build open_source"
echo "  - JNI 绑定编译会失败 (securec.h 路径问题), 不影响核心库和 UT"
echo "  - coverage-c++ 模式 (ASan) 全部12573测试可通过"
echo ""
