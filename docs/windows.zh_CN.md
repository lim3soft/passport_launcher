<p align="right"><strong>简体中文</strong> · <a href="windows.md">English</a></p>

# Windows 11 交接 — PassportOS 0.1.2

**用户当前选择：全新安装，不保留现有设置。** 请改用[浏览器刷写指南](web-flashing.zh_CN.md)。以下采集和导出步骤仅用于可选的旧设置迁移。

AI-Passport 连接您的 Windows 笔记本，本主机无法访问它。固件和主机测试通过；读取工具仅在 Linux 用模拟响应测试，**尚未在 Windows 或真实板子验证**。QSO 是独立 `.passport` 应用，启动器仅有设置导入支持，不含 QSO 无线电/UI。

## 1. 解压并准备

将整个 ZIP 解压到您控制的本地文件夹，在该文件夹打开 Windows Terminal/PowerShell。使用 Python 3.11–3.13（开发环境为 3.13）。若没有 `py`，可使用已有 ESP-IDF Python 终端，或从 [python.org](https://www.python.org/downloads/windows/) 安装 Python；遇到错误请反馈，不盲改驱动/安全设置。

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-windows.txt
.\.venv\Scripts\python.exe tools\windows_bundle.py verify .
```

独立环境固定 esptool 4.12.0，不影响其他刷写安装。直接调用解释器，无需激活 PowerShell 脚本或修改执行策略。见 [Espressif 安装指南](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/installation.html)。`build-validation.json` 保存构建主机路径作为来源；本地文件见 `bundle-manifest.json`。

## 2. 先识别 COM 端口

开机，使用 USB **数据线**，关闭占用设备的刷写工具/串口监视器，执行：

```powershell
.\.venv\Scripts\python.exe tools\windows_capture.py ports
```

只枚举、不打开端口。对照 Windows 设备管理器 → **端口（COM 和 LPT）** 连接前后变化，确认 AI-Passport，不选择其他板子。C3 原生 USB Serial/JTAG 通常显示 COM 端口，见 [Espressif 文档](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/usb-serial-jtag-console.html)。沿用笔记本已正常工作的驱动，包内不含驱动安装器。

## 3. 只读采集

将 `COM5` 改为真实端口，每次用新的输出目录：

```powershell
.\.venv\Scripts\python.exe tools\windows_capture.py capture --port COM5 --output private\capture-001
```

工具先校验包和版本，将明确选择的设备复位到 ROM 下载模式，只执行 `get_security_info`、`flash_id` 和限定范围的 `read_flash`。禁用 RAM stub 上传，多次读取间保持 ROM 模式，避免应用改动设置；速度可能较慢。不写 Flash/NVS/eFuse，不擦除、不刷固件、不执行迁移计划。

安全状态开启/不明、容量非 8 MB 或芯片修订超出镜像支持范围时停止。然后读取/验证旧 4 KiB 分区表扇区及 288 字节应用头。**仅自动支持已审计的 QSO Vision 2.3.3 构建与旧单应用布局**；不同固件/表会在读取凭据前停止，不要求全固件备份。

匹配时仅读取 24 KiB NVS，复核表，导出存在的最多 23 个支持设置字段，生成绑定本固件的审核计划。这只是审核证据，不是刷写许可。

完成后用硬件电源键关机再开机，恢复当前固件。停留 ROM 模式时黑屏/停止运行属预期。连接失败请保留错误，检查线缆和端口占用，不猜 BOOT 接线。

## 4. 只分享摘要

发送或粘贴 **`private\capture-001\report-to-share.json`**。摘要包含状态、COM、芯片/修订、容量、安全标志和哈希，不含 MAC、序列号、SSID、密码或原始日志。失败也先反馈摘要/错误。

私密保留 `legacy-nvs.bin`、`qso.qso-settings`、原始连接日志和 `migration-plan.json`。NVS/设置文件含 Wi-Fi 凭据，不上传整个目录。Windows 文件继承目录权限；不声称 POSIX `0600` 会配置 Windows ACL。

## 5. 审核后再刷写验收

不提供自动刷写脚本。首次安装改变旧分区，可能覆盖设置；计划可涉及初始化未来应用/数据区，任何写入前须核对实物、保留数据及具体范围。审核设备报告和计划后，再提供准确镜像、范围和 Windows 操作。

获准安装后：确认菜单，通过手机浏览器安装 `apps/` 中两个计数器，保存不同值，交替启动并核对保留和返回。QSO 另行安装，使用前将导出导入空的私有区。检查自动启动取消及复位。下载成功不等于实机验收；基础恢复/切换通过后，再在合适测试硬件进行掉电测试。

`firmware/PassportOS-full.bin` 是写 `0x0` 的首次配置镜像；`apps/*.passport` 只供启动器安装，不能刷到 `0x0`。匹配启动器调试文件在 `firmware/firmware/<sha256>/`。ZIP 不含个人采集数据或不可执行的生命周期协议样本。
