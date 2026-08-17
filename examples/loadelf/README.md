# NuttX SIM Loadable ELF Build and Test Guide

本文档说明如何从零搭建环境，clone `nuttx` 和 `nuttx-apps` 两个仓库，构建
NuttX SIM 固件，导出 `nuttx-export`，编译外部 ELF samples，并在 SIM 中通过
NSH 直接验证 ELF 和 JavaScript 脚本运行。

本文档使用的目录布局如下：

```text
workspace/
  nuttx/
  nuttx-apps/
```

`examples/loadelf/sim` 配置保留 ELF loader、NSH file apps、系统导出符号表等
运行能力。测试时把宿主机的 `../nuttx-apps` 通过 hostfs 挂载到 NuttX 里的
`/system`。

## 1. 安装依赖

Ubuntu/Debian 环境可以安装以下依赖：

```sh
sudo apt update
sudo apt install -y \
  build-essential git make cmake ninja-build \
  bison flex gettext texinfo gperf automake libtool pkg-config \
  libncurses-dev libncursesw5-dev libelf-dev genromfs \
  curl unzip xxd file \
  python3 python3-pip \
  gcc g++
```

当前配置使用 NuttX SIM 的 GCC 工具链。若本机默认 GCC 版本较旧，建议安装
GCC 14：

```sh
sudo apt install -y gcc-14 g++-14
```

不需要全局切换 GCC 时，可以在 `make` 命令中显式指定：

```sh
make -j"$(nproc)" CC=gcc-14 CXX=g++-14 CPP='gcc-14 -E -P -x c'
```

## 2. Clone 仓库

```sh
mkdir -p ~/nuttxspace
cd ~/nuttxspace

git clone https://github.com/apache/nuttx.git nuttx
git clone https://github.com/apache/nuttx-apps.git nuttx-apps
```

如果 loadable ELF 改动在你的 fork 或开发分支上，clone 对应 fork 后切到相同分支：

```sh
git -C nuttx checkout <nuttx-branch>
git -C nuttx-apps checkout <apps-branch>
```

## 3. 配置并构建 NuttX SIM 固件

从 `nuttx` 目录配置 `nuttx-apps/examples/loadelf/sim`：

```sh
cd ~/nuttxspace/nuttx

make distclean
./tools/configure.sh -l ../nuttx-apps/examples/loadelf/sim
make -j"$(nproc)"
```

构建成功后会生成：

```text
nuttx/nuttx
nuttx/nuttx.tgz
```

`nuttx` 是 SIM 可执行文件。此配置不需要 BSIM。

## 4. 导出 NuttX SDK

外部 ELF sample 不走 NuttX apps 编译框架，而是使用 `make export` 导出的头文件、
链接脚本和库。

```sh
cd ~/nuttxspace/nuttx

make export

loadelf_dir=../nuttx-apps/examples/loadelf
rm -rf "$loadelf_dir/nuttx-export"

export_tarball=$(ls -t nuttx-export-*.tar.gz | head -n1)
tar -xzf "$export_tarball" -C "$loadelf_dir"
mv "$loadelf_dir/${export_tarball%.tar.gz}" \
  "$loadelf_dir/nuttx-export"
```

导出目录最终应为：

```text
nuttx-apps/examples/loadelf/nuttx-export/
```

这个目录是本地生成物，不应提交到 git。

## 5. 编译外部 ELF samples

```sh
cd ~/nuttxspace/nuttx-apps
make -C examples/loadelf/samples
```

该命令会做两件事：

1. 编译 `examples/loadelf/samples/*/Makefile` 下的所有 ELF sample。
2. 安装 ELF 到 `nuttx-apps/bin/`，并从 ELF 未定义符号生成
   `examples/loadelf/symbols.txt`。

生成结果示例：

```text
nuttx-apps/bin/loadelf_sample
nuttx-apps/bin/loadelf_cpp_stress
nuttx-apps/examples/loadelf/symbols.txt
```

`symbols.txt` 会被固件编译进导出符号表。若 `symbols.txt` 发生变化，需要回到
`nuttx` 目录重新构建固件：

```sh
cd ~/nuttxspace/nuttx
make -j"$(nproc)"
```

## 6. 运行测试

启动 SIM：

```sh
cd ~/nuttxspace/nuttx
./nuttx
```

进入 NSH 后运行：

```sh
mount -t hostfs -o fs=../nuttx-apps /system
cd /system/bin
./loadelf_sample 10 20 30
./loadelf_cpp_stress
qjs /system/examples/loadelf/samples/loadelf_js_sample.js 10 20 30
poweroff
```

也可以用非交互方式一次性运行：

```sh
cd ~/nuttxspace/nuttx

printf '%s\n' \
  'mount -t hostfs -o fs=../nuttx-apps /system' \
  'cd /system/bin' \
  './loadelf_sample 10 20 30' \
  './loadelf_cpp_stress' \
  'qjs /system/examples/loadelf/samples/loadelf_js_sample.js 10 20 30' \
  'poweroff' | ./nuttx
```

期望结果：

```text
loadelf_sample: sum=60
loadelf_cpp_stress: failures=0
loadelf_js_sample: PASS
```

## 7. 文件路径关系

SIM 中的 `/system` 来自 hostfs：

```text
NuttX /system  ->  host ../nuttx-apps
```

因此这些路径是等价的：

```text
/system/bin/loadelf_sample
~/nuttxspace/nuttx-apps/bin/loadelf_sample

/system/bin/loadelf_cpp_stress
~/nuttxspace/nuttx-apps/bin/loadelf_cpp_stress

/system/examples/loadelf/samples/loadelf_js_sample.js
~/nuttxspace/nuttx-apps/examples/loadelf/samples/loadelf_js_sample.js
```

ELF sample 由 NSH 通过 `posix_spawnp()` 进入 NuttX binfmt/ELF loader；JS sample
不是 ELF，而是直接启动内置 `qjs`，再把脚本路径传给 QuickJS。

## 8. 单独检查 samples

检查 ELF 文件类型、头信息、section 和未定义符号：

```sh
cd ~/nuttxspace/nuttx-apps
make -C examples/loadelf/samples check
```

查看当前支持的 sample 目录：

```sh
make -C examples/loadelf/samples list
```

清理 sample 生成物：

```sh
make -C examples/loadelf/samples clean
rm -f bin/loadelf_sample bin/loadelf_cpp_stress
```

## 9. 常见问题

### `./sample: command not found`

通常是 `/system` 还没挂载、ELF 文件不存在，或者当前目录不对。

先在 NSH 里挂载 hostfs，再检查 `/system/bin`：

```sh
mount -t hostfs -o fs=../nuttx-apps /system
ls /system/bin
```

正确命令示例：

```sh
cd /system/bin
./loadelf_sample
./loadelf_cpp_stress
```

### `qjs` 找不到

确认固件配置包含 QuickJS，并重新构建固件：

```sh
grep QUICKJS ~/nuttxspace/nuttx/.config
make -C ~/nuttxspace/nuttx -j"$(nproc)"
```

进入 NSH 后也可以用 `help` 查看 Builtin Apps，正常应能看到 `qjs`。

### ELF 加载时报 undefined symbol

说明 ELF 引用了固件没有导出的符号。先重新生成 `symbols.txt`：

```sh
cd ~/nuttxspace/nuttx-apps
make -C examples/loadelf/samples
```

然后重新构建固件：

```sh
cd ~/nuttxspace/nuttx
make -j"$(nproc)"
```

如果是新 sample 引用了新的 NuttX API，需要确保该符号被加入导出表。

### `configure.sh` 刷新配置失败

如果本地 Python/Kconfig 环境有问题，可以先安装 Kconfig 依赖：

```sh
python3 -m pip install --user kconfiglib
```

然后重新运行：

```sh
cd ~/nuttxspace/nuttx
make distclean
./tools/configure.sh -l ../nuttx-apps/examples/loadelf/sim
```

## 10. 开发新 ELF sample

新增 sample 时，在下面新建一个带 `Makefile` 的子目录：

```text
nuttx-apps/examples/loadelf/samples/<your_sample>/
```

然后运行：

```sh
cd ~/nuttxspace/nuttx-apps
make -C examples/loadelf/samples
cd ../nuttx
make -j"$(nproc)"
```

运行：

```sh
./nuttx
nsh> mount -t hostfs -o fs=../nuttx-apps /system
nsh> cd /system/bin
nsh> ./<your_sample>
```

如果你的 sample 只是 JS 脚本，不需要编译，也不需要更新 `symbols.txt`；把脚本
放在 `examples/loadelf/samples/` 下，然后运行：

```sh
qjs /system/examples/loadelf/samples/your_script.js
```
