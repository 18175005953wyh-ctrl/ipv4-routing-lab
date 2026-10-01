# IPv4 Routing Lab

C11 命令行模拟器：计算 IPv4 子网信息，并从 CSV 路由表选择最长前缀匹配路由。
不需要联网，不调用操作系统网络 API，也不会改变本机路由表。

## 项目目标

通过可复现的输入、输出和自动化测试，理解 IPv4 位运算与路由选择。
两个子命令为 `subnet <IPv4/CIDR>` 和 `route <routes.csv> <destination-IP>`。

## IPv4的32位表示

每段 IPv4 数字占 8 位，四段共 32 位。例如 `192.168.1.10` 保存为 `0xC0A8010A`：

```c
address = ((uint32_t)a << 24) | ((uint32_t)b << 16)
        | ((uint32_t)c << 8) | (uint32_t)d;
```

这是整数的数值表示，不直接读取其内存字节，因而不依赖主机大小端。
解析器要求恰好四段十进制数，每段 0–255；拒绝符号、空格、尾随字符、空段和前导零
（单独的 `0` 允许，`01` 不允许），避免八进制歧义。格式化使用移位和 `& 255`。

## 子网计算规则

```c
mask = prefix == 0 ? 0 : UINT32_MAX << (32 - prefix);
network = address & mask;
broadcast = network | (uint32_t)~mask;
```

`/0` 单独处理，绝不对 32 位值移位 32 位。掩码中网络位为 1、主机位为 0，
因此按位与保留网络位、清零主机位。

| 前缀 | 本项目采用的传统可用主机规则 |
|---|---|
| /0–/30 | 数量为 `2^(32-prefix)-2`，范围 network+1 至 broadcast-1 |
| /31、/32 | 数量为 0，范围显示 N/A |

数量使用 `uint64_t`；`/0` 的传统计算值为 4294967294。
这只是地址位数的数学计算，未扣除保留地址、组播等特殊地址。
实际点对点链路可以特殊使用 `/31`，`/32` 也可表示单一主机路由；本项目的
“传统主机数为零”不表示它们不能用于路由匹配。

## 最长前缀匹配

匹配条件：`(destination & route.mask) == route.network`。
扫描全部路由，依次优先选择：**更长前缀 → 更小 metric → 更早 CSV 输入项**。
不排序、不修改原路由数组，查询为 O(n)，额外空间 O(1)。

默认路由 `0.0.0.0/0` 匹配任意 IPv4；即使 metric 更小，也不会抢过更具体的路由。
CSV 中 destination 的非零主机位加载时清零，例如 `10.1.2.37/24` 保存为 `10.1.2.0/24`。

## 路由表格式

`examples/routes.csv`：

```csv
destination,next_hop,interface,metric
0.0.0.0/0,192.168.1.1,eth0,100
10.0.0.0/8,direct,eth1,20
10.1.0.0/16,192.168.2.1,eth2,10
10.1.2.0/24,192.168.3.1,eth3,10
```

- 必须使用以上精确表头，四列无引号 CSV；不支持引号转义、逗号字段或多行字段。
- next_hop 为严格 IPv4 或小写 `direct`。不解析主机名，不验证下一跳可达性。
- interface 为 1–63 字节，不包含 ASCII 控制字符、DEL 或双引号；允许内部空格。
- metric 为 0–4294967295 的十进制整数；拒绝符号、空格和多位数前导零。
- 不自动去除字段空白。支持 LF、CRLF 和末行无换行；不支持 UTF-8 BOM。
- 最多 256 条路由，每行最多 1023 字节（不含 LF）；空文件、仅表头、空数据行均拒绝。
- 错误带 CSV 行号；文件打开失败标为 line 0。任何损坏行使整个加载失败，
  不会仅用已读取的正确部分继续查询。成功后才交付完整表。

## 文件结构

```text
include/ipv4.h          IPv4、CIDR、子网结构与接口
include/route.h         路由结构、查询接口
include/route_reader.h  CSV 加载接口
src/ipv4.c             严格解析、格式化、掩码与子网计算
src/route.c            只读最长前缀查询
src/route_reader.c     有界 CSV 读取与验证
src/main.c             参数验证、输出、退出码
tests/test_runner.c    无文件依赖的核心检查
tests/test_cli.py      真实进程、CSV、退出码检查（Python 标准库）
examples/routes.csv    固定演示路由表
.github/workflows/ci.yml
```

## 构建与运行

需要 C11 编译器和 CMake 3.20+；默认测试需要 Python 3，未安装时可用
`-DBUILD_TESTING=OFF` 仅构建程序。应用本身不依赖 Python，也无第三方库。

通用构建（从仓库根目录）：

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Visual Studio 多配置生成器通常生成 `build/Debug/ipv4-routing-lab.exe`；
NMake 生成器使用 `build/ipv4-routing-lab.exe`；Linux 通常为 `build/ipv4-routing-lab`。
不同生成器请使用不同构建目录。

本地实际验证环境：Windows x64、MSVC 19.44.35225、CMake 3.31.6-msvc6、Python 3.12.14。
在 **x64 Native Tools Command Prompt for VS 2022** 中执行的配置与测试命令：

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=/WX -DPython3_EXECUTABLE=C:/Users/wyh58/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe
cmake --build build
ctest --test-dir build --output-on-failure -V
```

最后一项 Python 路径属于本次验证机器，其他机器应替换或在 Python 已加入 PATH 时省略。
CMake 为 MSVC 启用 `/W4`，为 GCC/Clang 启用 `-Wall -Wextra -Wpedantic`；
本次还启用 `/WX` 将警告视为错误。

## 示例输出

以下两组输出来自本地实际执行。

```text
build\ipv4-routing-lab.exe subnet 192.168.10.37/24
Address:        192.168.10.37
Prefix:         /24
Subnet mask:    255.255.255.0
Network:        192.168.10.0
Broadcast:      192.168.10.255
First host:     192.168.10.1
Last host:      192.168.10.254
Usable hosts:   254
```

```text
build\ipv4-routing-lab.exe route examples/routes.csv 10.1.2.45
Destination:    10.1.2.45
Matched route:  10.1.2.0/24
Next hop:       192.168.3.1
Interface:      eth3
Metric:         10
```

`10.1.2.45` 同时匹配 `/0`、`/8`、`/16`、`/24`，其中 `/24` 固定了最多网络位、
覆盖范围最小，所以选中 eth3。结果由 CSV 决定，不是硬编码为某个下一跳。

## 自动化测试

2026-10-01 本地 Windows 严格编译与测试结果：

```text
Core checks: 62, failures: 0
Ran 43 tests in 1.347s
OK
100% tests passed, 0 tests failed out of 2
Total Test time (real) =   2.39 sec
```

共 **62 个 C 核心断言检查 + 43 个 Python 命令行测试**，由 **2 个 CTest 测试项**运行。
耗时会随机器变化。CLI 测试在构建目录创建独立临时夹具，不修改 examples。

覆盖 IPv4/CIDR 非法输入、十进制溢出、格式往返、/0、/30、/31、/32、
主机位规范化、无序路由、metric 并列、输入顺序、查询不变性、默认路由、无匹配、
CSV 行号、损坏行、超长行、NUL、CRLF、空文件、路由容量和命令行参数数量。

## GitHub Actions

已配置 push、pull_request 和手动触发；Ubuntu 与 Windows 两个任务执行配置、编译、CTest。
使用官方 [checkout v6](https://github.com/actions/checkout/blob/v6/README.md) 和
[setup-python v6](https://github.com/actions/setup-python/blob/v6/README.md)，权限仅 contents: read。

**交付时尚未发布本仓库，远程 CI 尚未运行，不能视为已经变绿。**
发布后检查 Actions 中 `CMake and CTest` 的两个任务；确认真实成功后才能更新验证状态。
目前不放成功徽章，Linux 运行结果也仍待验证。

## 边界情况

| 情况 | 行为 |
|---|---|
| --help | 用法输出到 stdout，退出 0 |
| 缺少或多余参数、未知子命令 | 用法输出到 stderr，退出 2 |
| 非法 IP/CIDR/CSV、读取失败 | 简短错误输出到 stderr，退出 2 |
| 有效表中没有匹配路由 | 明确提示，退出 3 |
| 重复网络与 metric | 保留最早输入项 |
| /31、/32 | 传统主机范围 N/A，仍可正常参与路由查询 |

## 当前限制

- 教学模拟器，不处理 IPv6、Socket、实际发包、真实操作系统路由和 GUI。
- 仅线性扫描，适合最多 256 条路由；不使用 trie，不模拟等价多路径或递归下一跳。
- CSV 为明确受限的四列格式，不支持通用带引号 CSV；接口名称按字节保存，
  不验证 UTF-8 序列。Windows 文件路径使用 C 运行时窄字符 API，非本地编码路径可能失败。
- 不基于特殊地址类别额外调整主机数量，不模拟现实网络管理策略。
- 本机已通过 MSVC；Ubuntu/Windows GitHub 托管运行器仍待发布后实际验证。
