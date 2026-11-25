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

# 统一配置加载器：供门禁 / 版本构建两类场景共用。
# 取值优先级：CI 显式入参(环境变量) > [build] 默认值 > 脚本内置兜底。
# 门禁(PR)无法手动入参，因此环境变量为空时自然回落到 build.ini 默认值。

set -e

SCRIPT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_ROOT}/../.." && pwd)
INI_FILE="${INI_FILE:-${REPO_ROOT}/build.ini}"

# 需要在两类场景间统一解析的配置项；module 由工作流直接注入，不参与本表
CONFIG_KEYS="
OmniOperatorJIT_Version Product_Version
jdk_version OMNI_COMPILER_THREAD_COUNT
"

get_ini() {
    local section="$1"
    local key="$2"
    [[ -f "${INI_FILE}" ]] || return 0
    awk -v sec="${section}" -v k="${key}" '
        /^\[.*\]/ { in_sec=($0 == "[" sec "]") }
        in_sec && match($0, "^[[:space:]]*" k "[[:space:]]*=[[:space:]]*(.*)", arr) {
            gsub(/[[:space:]]+$/, "", arr[1])
            gsub(/^["\047]|["\047]$/, "", arr[1])
            print arr[1]
            exit
        }
    ' "${INI_FILE}"
}

# 最后一层兜底：build.ini 缺键时仍可跑通最小闭环
_default_value() {
    case "$1" in
        OMNI_COMPILER_THREAD_COUNT) echo "32" ;;
        *) echo "" ;;
    esac
}

load_build_config() {
    local key value origin
    for key in ${CONFIG_KEYS}; do
        value="${!key:-}"
        origin="env"
        if [[ -z "${value}" ]]; then
            value="$(get_ini build "${key}")"
            origin="[build]"
        fi
        if [[ -z "${value}" ]]; then
            value="$(_default_value "${key}")"
            origin="builtin"
        fi
        [[ -z "${value}" ]] && origin="unset"
        export "${key}=${value}"
        CONFIG_SOURCES["${key}"]="${origin}"
    done

    compute_module

    # WORKSPACE：产物落到 ${WORKSPACE}/output/<module>。平台未注入时取仓库父目录，
    # 不依赖 checkout 的目录名（门禁是 OmniOperatorJIT，版本构建是 OmniOperator）。
    export WORKSPACE="${WORKSPACE:-${REPO_ROOT%/*}}"
}

# require_config key... —— 场景必填项缺失时一次性列出并退出
require_config() {
    local missing=()
    local key
    for key in "$@"; do
        [[ -z "${!key:-}" ]] && missing+=("${key}")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "ERROR: required config is empty: ${missing[*]}" >&2
        echo "       pass it as an env var (CI input) or set it in build.ini ([build] as default)" >&2
        exit 1
    fi
}

print_config() {
    echo "===== build config ====="
    echo "INI_FILE=${INI_FILE}"
    echo "WORKSPACE=${WORKSPACE}"
    local key
    for key in ${CONFIG_KEYS}; do
        printf '  %-28s %-8s %s\n' "${key}" "(${CONFIG_SOURCES[$key]:-unset})" "${!key:-<empty>}"
    done
    echo "module      = ${module:-<empty>}"
    echo "==============================================="
}

# 编译脚本末尾会统计 ccache；镜像未装 ccache 时不应让构建失败
set_ccache() {
    mkdir -p /home/jenkins/agent/build_cache/.ccache
    sudo chown -R jenkins:jenkins /home/jenkins/agent/build_cache
    sudo chmod -R 755 /home/jenkins/agent/build_cache || true
    command -v ccache >/dev/null 2>&1 || return 0
    export CCACHE_DIR="/home/jenkins/agent/build_cache/.ccache"
    mkdir -p "${CCACHE_DIR}"
    ccache -M 0 >/dev/null 2>&1 || true
    export CCACHE_COMPRESS=true
    export CCACHE_HASHDIR=true
    export PATH="${CCACHE_DIR}/bin:$PATH"
    export PATH="/usr/lib64/ccache:$PATH"
}

# module 标识：openEuler22.03_JDK8 这类目录名，决定产物归档目录 output/<module>
# 版本构建工作流会显式传入；未传时由 /etc/os-release 的 PRETTY_NAME 与 jdk_version 推导作为兼容回落
compute_module() {
    local pretty os_ver=""
    # os_version 独立推导：OmniOperator_build.sh 解 OmniOperator 包时按 ${os_version}_JDK* 目录名取值，
    # 不能因为 CI 显式传了 module 就跳过
    if [[ -r /etc/os-release ]]; then
        pretty=$(grep PRETTY_NAME /etc/os-release | cut -d= -f2)
        os_ver=$(printf '%s' "${pretty}" | sed -E 's/"([A-Za-z0-9]+) ([0-9]+\.[0-9]+).*/\1\2/')
        [[ "${os_ver}" == "${pretty}" || -z "${os_ver}" ]] && os_ver=$(printf '%s' "${pretty}" | tr -d '"()' | awk '{print $1 $2}')
    else
        echo "WARN: 无 /etc/os-release，os_version 置空" >&2
    fi
    export os_version="${os_ver}"
    # 允许 CI 显式传入 module（每个组合任务写死一个），否则由系统版本推导
    if [[ -z "${module:-}" && -n "${jdk_version:-}" ]]; then
        export module="${os_ver:+${os_ver}_JDK${jdk_version}}"
    fi
}

declare -A CONFIG_SOURCES
load_build_config
