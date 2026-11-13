# OmniOperator CI 构建镜像

GitCode Action 流水线使用的 aarch64 构建镜像定义，覆盖 openEuler 22.03 LTS SP3 与 24.03 LTS SP3 两个基线。镜像预置了 OmniOperator 编译所需的工具链与全部三方库，工作流只需传入镜像名（`docker_img_*`）即可直接编译。

## 目录结构

```text
.gitcode/dockerfiles
├── scripts
│   ├── install-dependencies.sh   # 统一安装入口：基础包 / 构建工具 / 三方库 / maven 配置
│   └── install-buildtools.sh     # 工具与 settings.xml 的安装函数，由上面的脚本 source
├── openeuler2203
│   ├── Dockerfile_01             # 第一层：基础包 + 构建工具
│   ├── Dockerfile_02             # 第二层：三方库 + jenkins 运行用户
│   └── Dockerfile_arm_all        # 单层合并版本（= 01 + 02）
└── openeuler2403
    ├── Dockerfile_01
    ├── Dockerfile_02
    └── Dockerfile_arm_all
```

## 镜像分层

| 文件 | 起点镜像 | 内容 |
| --- | --- | --- |
| `openeuler2203/Dockerfile_01` | `swr.cn-north-4.myhuaweicloud.com/cloud_boostkit/openeuler22.03_lts_sp3:latest` | 重写 yum 源，装基础包与全部构建工具 |
| `openeuler2203/Dockerfile_02` | `.../cloud_boostkit/openeuler_22.03_lts_sp3:omnioperator_arm_01` | 写 maven 配置，装全部三方库，创建 jenkins 用户 |
| `openeuler2403/Dockerfile_01` | `.../cloud_boostkit/openeuler-24.03-lts-sp3:latest` | 同上，仓库地址指向 openEuler-24.03-LTS-SP3 |
| `openeuler2403/Dockerfile_02` | `.../cloud_boostkit/openeuler_24.03_lts_sp3:omnioperator_arm_01` | 同上 |
| `*/Dockerfile_arm_all` | 各自的 `latest` 基础镜像 | 把 01、02 合并为一个 Dockerfile，便于一次性构建或验证 |

第一层只装工具，体积稳定、极少变动；第二层装库，是日常迭代的主要对象。改动三方库版本时只需重建第二层，第一层可直接复用。

`Dockerfile_01` 的 yum 源使用 openEuler 官方 aarch64 仓库（everything / EPOL main / update），并关闭 gpgcheck。

## 预装内容

### 构建工具

| 工具 | 版本 | 安装位置 |
| --- | --- | --- |
| bisheng JDK | 17.0.18 | `/opt/buildtools/bisheng-jdk-17.0.18`，`/etc/profile` 中 `JAVA_HOME` 指向它 |
| bisheng JDK | 8u342 | `/opt/buildtools/bisheng-jdk1.8.0_342` |
| CMake | 3.28.2 | `/usr/local/bin` |
| Maven | 3.9.9 | `/opt/buildtools/apache-maven-3.9.9` |
| LLVM | 15.0.4 | `/usr/local`（含 clang、lld、libunwind、compiler-rt、lldb） |
| lcov | 1.16 | 以 rpm 方式安装 |

两个 JDK 都会装满，运行期由 `.gitcode/scripts/OmniOperator_build.sh` 的 `config_env`（`jdk17_install` / `jdk8_set`）按工作流的 `jdk_version` 选择。`Dockerfile_02` 里 `export JAVA_HOME=...jdk1.8.0_342` 只对当层 RUN 生效（供 hadoop 的 mvn 编译使用），不会固化进镜像环境。

### 三方库

| 组件 | 版本 | 说明 |
| --- | --- | --- |
| googletest | 1.12.0 | 单元测试框架，静态库；`__int128` 的 `PrintTo` 从 1.12 才有，UT 里 `EXPECT_EQ` 比较 128 位整数依赖它 |
| jemalloc | 5.3.0 | `--with-lg-page=16`，与 arrow 的页大小配置一致 |
| nlohmann-json | 3.11.3 | header-only |
| snappy | 1.1.10 | 取 openEuler src 包，打 RTTI 与去 benchmark 补丁，共享库 |
| libboundscheck | 1.1.16 | 安全函数库，仅保留 `include/`、`lib/` |
| abseil-cpp | 20250127.0 | C++17，静态 PIC |
| re2 | 2024-07-02 | C++17，共享库 |
| rapidjson | 1.1.0 | header-only |
| protobuf | 3.21.9 | 独立前缀 `/opt/buildtools/Protobuf-3.21.9`，导出 `PROTOBUF_HOME` |
| zlib | 1.3.1 | |
| zlib-ng | 2.0.4 | 静态 PIC，arrow / orc 静态链接时以 `-lz-ng` 使用 |
| lz4 | 1.10.0 | |
| zstd | 1.5.6 | |
| cyrus-sasl | 2.1.28 | hadoop / orc 的 SASL 依赖 |
| jsoncpp | 1.9.6 | 静态库，orc 需要 |
| arrow | 11.0.0 | 打 boostkit-bigdata 的 `arrow-maint-11_0_0` 补丁，开启 dataset / substrait / HDFS / mimalloc |
| orc | 1.7.4 | 华为 codehub 的 `v1.7.4_new` 分支，静态聚合后重打成 `liborc.so` |
| hadoop | 3.2.0 | 只编 libhdfs native client，打 boostkit 补丁，头文件与 so 落 `/usr/local` |
| fmt | 10.1.1 | 共享库 |
| folly | 2024.07.01.00 | 静态 PIC；double-conversion / libevent / glog 由 yum 提供 |

除 protobuf 装在独立前缀、libboundscheck / rapidjson 留在 `/opt/buildtools` 下之外，其余库的 `make install` 产物统一在 `/usr/local`（`include` / `lib` / `lib64`）。源码来源为 gitcode / atomgit / gitee 镜像与 github 官方仓库，orc 取自华为 codehub；arrow 与 hadoop 的补丁从 `gitee.com/kunpengcompute/boostkit-bigdata` 拉取，打完即删，不保留 clone。

各安装函数在收尾时都会删除源码目录，`install_all_libs` 末尾还会把 `/opt/buildtools`、`/usr/local` 的体积写进构建日志，便于后续瘦身。

## 安装脚本用法

```bash
bash scripts/install-dependencies.sh --install-base-pkgs
bash scripts/install-dependencies.sh --install-all-tools
bash scripts/install-dependencies.sh --install-all-libs
bash scripts/install-dependencies.sh --install <component>
bash scripts/install-dependencies.sh --setup maven
bash scripts/install-dependencies.sh --help
```

- `--install <component>`：安装单个组件，目标名取自脚本内 `supported_tools` / `supported_libs` 数组，例如 `arrow-11.0.0`、`bisheng-jdk-8u342`。
- `--install-base-pkgs`：yum 基础包 + pip 包（pandas、tqdm、openpyxl、pyinstaller、fastcov、pre-commit，走阿里云镜像）。`google-benchmark-devel` 也从这里装，来自 EPOL（1.6.1，UT 的 benchmark 用例用）。
- `--install-all-tools` / `--install-all-libs`：按数组顺序全量安装。
- `--setup maven`：写入 `/opt/buildtools/apache-maven-3.9.9/conf/settings.xml`，本地仓库为 `/root/build_cache/.maven_cache`，镜像走华为云，`external:*.googleapis.com` 回落 Maven Central。

版本升级只需在脚本里新增或替换对应函数并更新数组，Dockerfile 无需改动。函数名由目标名将 `.`、`-` 替换为 `_` 得到（`bisheng-jdk-8u342` → `install_bisheng_jdk_8u342`）。

## 构建镜像

构建上下文必须是仓库根目录，因为 Dockerfile 中的 `COPY .gitcode/dockerfiles/scripts` 以仓库根为基准。镜像为 arm64，x86 机器需 buildx + QEMU。

```bash
# 第一层（产物须打第二层 FROM 引用的标签）
docker buildx build --platform linux/arm64 --load \
  -f .gitcode/dockerfiles/openeuler2203/Dockerfile_01 \
  -t swr.cn-north-4.myhuaweicloud.com/cloud_boostkit/openeuler_22.03_lts_sp3:omnioperator_arm_01 .

# 第二层
docker buildx build --platform linux/arm64 --load \
  -f .gitcode/dockerfiles/openeuler2203/Dockerfile_02 \
  -t swr.cn-north-4.myhuaweicloud.com/cloud_boostkit/openeuler_22.03_lts_sp3:omnioperator_arm_02 .

# openEuler 24.03 只需替换目录与标签中的版本号，第一层标签为
# swr.cn-north-4.myhuaweicloud.com/cloud_boostkit/openeuler_24.03_lts_sp3:omnioperator_arm_01
```

`Dockerfile_arm_all` 用同样的方式构建，一次完成全部安装（耗时更长、无层缓存可复用）。

推送标签要与工作流保持一致：`.gitcode/workflows` 中的 `docker_img_openeuler2203` / `docker_img_openeuler2403` 显式指定镜像，仓库内其他配置不写死镜像名。

## 运行期约定

- 用户：`jenkins`（UID/GID 1000，免密 sudo），`WORKDIR /home/jenkins/agent`，声明卷 `/home/jenkins/.jenkins`、`/home/jenkins/agent`。
- 编码：`LANG`、`LC_ALL`、`LANGUAGE` 均为 `C.utf8`。
- `/etc/profile` 中累积了 `JAVA_HOME`、`PROTOBUF_HOME` 以及各类 `PATH` / `LD_LIBRARY_PATH`，非登录 shell 不会加载；CI 统一由 `OmniOperator_build.sh` 的 `config_env` 显式导出，路径按本文件的安装位置写死，镜像内目录名需与之一致。
- `Dockerfile_02` 结束前会清掉 `/root/build_cache/` 与 `/tmp`，镜像内不携带 maven 本地仓库缓存。
