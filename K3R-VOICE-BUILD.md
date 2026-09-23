# K3R-voice — Weasel fork 构建说明

本仓库是 [rime/weasel](https://github.com/rime/weasel) **0.17.4** 的 fork，
目的：在「倾听输入法」的托盘右键菜单和设置窗口里加一个 **倾听输入法 设置** 入口
（打开同套件里的 `listen.exe --settings`），并关闭上游的自动更新检查。

- `master` = 上游 0.17.4（tag `0.17.4`，commit `9cc96e2`），**未改动**
- `aivoice` = 本 fork 的单提交

## 1. 与上游的差异

| 文件 | 改动 |
|------|------|
| `include/afxres.h` | **新增**：MFC 未安装时的资源头 shim（内容等价 `<winres.h>`） |
| `WeaselServer/resource.h`、`include/resource.h` | 新增 `ID_WEASELTRAY_AIVOICE 40017` |
| `WeaselServer/WeaselTrayIcon.cpp` | `CustomizeMenu()` 里在「输入法设定」下方插入「倾听输入法 设置 (&V)」 |
| `WeaselServer/WeaselServerApp.cpp` | 注册该菜单项 → `LaunchListenSettings()`；注释掉 WinSparkle 初始化/清理 |
| `WeaselServer/WeaselServerApp.h` | `check_update()` 改为弹「本版本已关闭更新检查（K3R-voice）」 |
| `include/WeaselUtility.h` | **新增** `LaunchListenSettings(HWND)`：自己查 App Paths（`listen.exe`，HKLM/HKCU × 64/32 四视图）拿完整路径再 `ShellExecuteW(…,"--settings")`，失败弹「未找到倾听输入法，请先安装。」 |
| `WeaselSetup/resource.h`、`InstallOptionsDlg.{h,cpp}` | 【倾听输入法】安装选项 底部加「倾听输入法 设置」按钮（动态创建） |
| `WeaselDeployer/resource.h`、`SwitcherSettingsDialog.{h,cpp}` | 【倾听输入法】方案选单设定（托盘「输入法设定」）底部加同一按钮（动态创建） |
| `patches/boost-1.84.0-msvc-14.5.patch` | **新增**：让 Boost 1.84 支持 MSVC 14.5x / v145 |

设计取舍：
- **不改 `.rc` 资源**：菜单项用上游预留的 `CSystemTray::CustomizeMenu(HMENU)` 钩子，
  按钮用 `CreateWindowExW` 动态创建 —— 少动上游文件，也避免 UTF-16 `.rc` 的编辑问题。
- **入口不复制设置项**：fork 侧只做「唤起倾听输入法 设置窗」，设置项仍在 Listen 里。

## 2. 工具链（已验证组合）

| 组件 | 版本 / 路径 |
|------|------------|
| Visual Studio | **VS 2026 (18.4.3) Community**，MSVC **14.50.35717**，平台工具集 **v145** |
| CMake | 4.2.3（VS 自带：`…\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`） |
| Boost | **1.84.0** 源码（已打 `patches/` 里的补丁），构建产物在 `stage\lib` |
| NSIS | 3.10 便携版（`Bin\makensis.exe`） |
| Python / Git | 任意；`bash`（Git 自带）供 `plum/rime-install` 用 |

> 上游默认 v142/v143；本机只有 14.50 且无管理员权限装 v143，故改用 v145。
> Boost 1.84 的 Boost.Build 只认到 14.3，因此需要 `patches/boost-1.84.0-msvc-14.5.patch`。

## 3. 构建步骤

```bat
rem ---- 0) 依赖 ----
rem  a) Boost 1.84.0 源码解压到 K:\Libraries\boost-1.84.0
rem  b) 打补丁： cd /d K:\Libraries\boost-1.84.0 && git apply -p1 <本仓库>\patches\boost-1.84.0-msvc-14.5.patch
rem  c) 生成头文件树 + 编库（只需 7 个库；x64 + x86 都要，Weasel 的 Win32 配置要用）
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d K:\Libraries\boost-1.84.0
bootstrap.bat
b2.exe headers
set BJAM=-j16 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread define=BOOST_USE_WINAPI_VERSION=0x0603 toolset=msvc-14.5 link=static runtime-link=static --build-type=complete
b2.exe %BJAM% architecture=x86 address-model=64 stage
b2.exe %BJAM% architecture=x86 address-model=32 stage

rem ---- 1) librime（必须带 3 个插件，否则雾凇拼音会报 lua_processor/lua_filter 错）----
cd /d <本仓库>\librime
git clone --depth 1 https://github.com/hchunhui/librime-lua.git plugins\lua
git clone --depth 1 https://github.com/lotem/librime-octagram.git plugins\octagram
git clone --depth 1 https://github.com/rime/librime-predict.git plugins\predict
git clone https://github.com/hchunhui/librime-lua.git -b thirdparty --depth=1 plugins\lua\thirdparty

rem ---- 2) 环境变量（env.bat，见下）----
rem ---- 3) 一次编完 librime(x64+x86) + Weasel(x64+Win32) ----
cd /d <本仓库>
set CMAKE_POLICY_VERSION_MINIMUM=3.5   &rem CMake 4.x 编 yaml-cpp 等老依赖需要
set RELEASE_BUILD=1                    &rem 让版本号固定为 0.17.4.0（不带 git hash）
call build.bat rime weasel

rem ---- 4) NSIS 出包 ----
<K3R-voice>\..\nsis-3.10\Bin\makensis.exe /DWEASEL_VERSION=0.17.4 /DWEASEL_BUILD=0 /DPRODUCT_VERSION=0.17.4.0 output\install.nsi
```

### env.bat（放在仓库根，`build.bat` 会自动调用）

```bat
set WEASEL_ROOT=<本仓库路径>
set BOOST_ROOT=K:\Libraries\boost-1.84.0
set BJAM_TOOLSET=msvc-14.5
set CMAKE_GENERATOR="Visual Studio 18 2026"
set PLATFORM_TOOLSET=v145
set DEVTOOLS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Git\cmd;C:\Program Files\Git\usr\bin;
```

`librime\env.bat` 同内容（`RIME_ROOT` 指向 `librime`，**不要**在里面设 `ARCH`，
`Weasel` 的 `build.bat` 会按平台设置它）。

## 4. 产物

| 产物 | 路径 |
|------|------|
| x64 可执行/IME | `output\`（WeaselServer.exe / WeaselSetup.exe / WeaselDeployer.exe / weaselx64.ime / weaselx64.dll / rime.dll / WinSparkle.dll） |
| x86 可执行/IME | `output\Win32\`（Win32 平台用；x64 系统只需其中的 `weasel.ime` / `weasel.dll`） |
| 安装包 | `output\archives\weasel-0.17.4.0-installer.exe` |

安装包内容与官方 0.17.4 包对齐（含 `data\` 方案与 `data\opencc`）；
差异：不含 ARM 产物（本机 x64 不需要），二进制因用 14.50 编译体积略大。

## 5. 已知环境差异（不是 fork 引入的问题）

- `WeaselDeployer.exe /deploy` 在本机当前环境下会长时间卡住（日志停在加载
  配置中途、CPU 空闲）。**官方 0.17.4 二进制同样卡住**，判断与环境（无交互桌面 /
  托盘通知）有关，与本 fork 无关。
- 本机 VS 未装 MFC，上游 `.rc` 的 `#include "afxres.h"` 会 RC1015；
  `include/afxres.h` shim 即为此准备（装上 MFC 后可删）。
