<p align="right"><strong>简体中文</strong> · <a href="research.md">English</a></p>

# 硬件证据与架构决策

> 历史架构：下文固定槽决策适用于 0.1–0.3；硬件证据仍有效，0.4.0 改用[共享存储](shared-storage.zh_CN.md)。

审计日期：2026-10-27。区分本地产品/BSP 证据、芯片能力与仍待测量的参数。PassportOS 是类似操作系统的固件启动器，不提供并发进程或特权沙箱。

## 本地证据

只读检查 `/root/ai-passport`，其基线为 `006c2279852e352a8433b2c21e5e09be2ca36f77`，分支 `feature/brandmeister-monitor` 已有大量修改，全部保留。QSO 来源为干净的 `/root/qso-ai-passport-publish`，版本 `b4ecee1295dd5ad9d8b1c95e1b5f6f91e5cbd4a2`。审核了产品规格、硬件指南、`bsp_pins.h`、BSP 实现、分区表、sdkconfig/defaults、构建 JSON 与发布归档，不将旧增量构建当作最终版本。文件哈希见 `evidence.json`。

| 项目 | 已有证据 | 仍待核实 |
| --- | --- | --- |
| MCU | ESP32-C3 单核 RISC-V，构建目标 `esp32c3` | 具体订货料号/模块、主板及芯片修订；最低支持 0.3 并非测得修订 |
| Flash | 产品 8 MiB，DIO/80 MHz/8 MiB 镜像配置 | 实物 JEDEC、容量、型号与稳定时序 |
| 内存 | 无 PSRAM；C3 有 400 KiB SRAM（含 16 KiB 缓存）与 8 KiB RTC SRAM | 上传/QSO 峰值堆及最大 DMA 块 |
| 显示 | ST7789P3，240×320 竖屏 RGB565，SPI2 四线，80 MHz/mode 0 | 实际显示与电气时序 |
| 输入 | 上/下/OK 共用 GPIO0 ADC1_CH0，独立硬件电源键 | 多板/电量下阈值分布 |
| USB | Type-C 5 V，GPIO18/19 原生 USB Serial/JTAG | 线缆、枚举、恢复接入及 eFuse |
| 无线 | 产品为 2.4 GHz 802.11 b/g/n、Bluetooth 5 LE；不支持经典蓝牙 | 实板 SoftAP/无线表现；启动器禁用 BLE |
| 启动 | ROM → `0x0` 二级引导器，`0x8000` 分区表，旧应用 `0x10000` | 实际复位与恢复 |
| 旧分区 | NVS `0x9000/0x6000`，PHY `0xf000/0x1000`，factory `0x10000/0x7f0000`，无 OTA | 目标设备当前表可能不同 |
| 工具 | ESP-IDF 5.5.3、CMake/Ninja、esptool 4.12.0、RISC-V GCC 14.2.0、锁定 LVGL 9.6.0~1 | 不代表实物修订 |
| 持久化 | QSO 设置与 Wi-Fi 档案都在 `monitor`，通过默认 `nvs_open`；SDK Wi-Fi 为 RAM；历史在内存 | 首次迁移的导出/导入 |
| 安全 | 现有构建关闭回滚、安全启动及 Flash 加密 | 未读取或烧写实物 eFuse |

芯片内存、无线及 USB 能力来源于 [Espressif C3 数据手册](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)。固定 USB Serial/JTAG 不是通用 OTG 控制器，不能承诺原生 U 盘、HID 或 USB 主机安装。已实现手机 Wi-Fi 浏览器路径；未假设或增设 SD 卡。

### 驱动复用与初始化

保留 BSP 和许可证。LCD 为 MOSI9、SCLK8、CS1、DC20，无 MCU 硬复位，保留反色及厂家寄存器；背光 GPIO21 使用 5 kHz/10 位 LEDC，不能兼作 UART0 TX。保留字节交换、圆角掩码及 LVGL 锁。实际实现为 **40 行**缓冲而非指南旧描述的 20 行，DMA 为 `240×40×2=19,200` 字节，LVGL 池 24 KiB。

按键保留 ADC 校准、外部 10 kΩ 上拉、`[0,150)`、`[150,447)`、`[447,1900)` mV 窗口及 180/500 ms 短/长按时序，回调只派发事件。单 ADC 不支持任意同时按键。指南称 GPIO0 与启动有关，但 C3 手册的绑带引脚是 GPIO2/8/9，**不推断 GPIO0 可进入 ROM 下载模式**；恢复接线须实测。

I2C SDA10/SCL7 由 ES8311 `0x18` 与 CW2017 `0x63` 共用，I2S 为 MCLK6/BCLK5/WS3/DOUT2/DIN4，功放常开假设不变。启动器仅初始化显示、按键和可选电池；QSO 保留音频。按键回调不启动慢操作。关机电平和复位后的外设恢复仍待实测。

### 文件实测，不是硬件实测

QSO Vision 2.3.3 合并镜像为 **1,894,144 字节**，SHA-256 `8f9a2198fd5d764945cd96931fb7b1e81d4c7ee986e1b6e99f44079e9005a483`。匹配归档的纯应用为 **1,828,608 字节**（约 1.744 MiB），SHA-256 `6ab6aec30d4453a663f5de40419de9764c9c3586cae0bced8f58881612563228`，因此加入 SDK 前距 2 MiB 尚有 **268,544 字节**。旧增量应用大小不同，未用于容量判断。新构建大小/哈希见归档 manifest 与验证结果。

## 架构比较

预留 64 KiB 启动/表/系统区、1.5 MiB 启动器与 448 KiB 元数据/私有数据，剩 **6 MiB** 存固件。以下按未压缩 2 MiB 应用估算，包元数据也占空间。小镜像可支持其他布局，但当前固定表不能凭空增加第四位置。

| 项目 | A：专用 OTA 槽 | B：仓库 + 一个 2 MiB 执行槽 | C：仓库 + 两个 2 MiB 执行槽 |
| --- | --- | --- | --- |
| 容量 | 三个独立 2 MiB 应用 | 仓库至多两个接近 2 MiB 镜像；选中者在执行槽重复，元数据使上限略降 | 仓库扣除元数据后不足完整 2 MiB；全尺寸保留及交换空间不足 |
| 切换 | 校验、选 OTA、重启，无代码复制 | 未命中时重写最多 2 MiB | 缓存命中快，否则写非活动槽 |
| 磨损 | 安装更新和少量启动选择写入 | 反复切换导致整槽擦写 | 命中时少，未命中多于 A |
| 数据 | 稳定私有分区，无交换 | 仍需稳定私有分区 | 同样要求 |
| 可靠性 | 更新中断不伤启动器，但该应用需重装 | 复制中断可恢复；仓库本身需事务/垃圾回收 | 可保留旧执行镜像，但仓库仍需暂存空间 |
| 兼容/隔离 | 合作式 IDF/ABI，无任意固件沙箱 | 复制不解决数据碰撞或绝对地址 | 额外槽不提供安全隔离 |
| 维护 | 固定映射，SDK OTA，归属简单 | 增加分配器、回收、复制/恢复日志 | 缓存与恢复策略最复杂 |

**0.1 选择 A，并保留 factory 恢复启动器。** 常驻启动器有时被称为混合特性，但这里明确没有内部固件仓库，因此归类 A。可执行分区本身保留镜像，手机可保留额外安装包；已安装应用切换不需要手机。四个接近 QSO 大小的镜像无法与当前预留共存。超过 2 MiB 或需要更多资源数据时，须减少应用数、缩减资源或使用经核实的大容量硬件，不能假设硬件升级必然可用。

未宣称测得切换延迟。A 需要哈希读取，B/C 还取决于擦写吞吐。每个应用都保留安全双副本需要更多 Flash。中断恢复保护启动器与其他应用，**不保留被更新应用的旧版本**。

### 固定布局

| 分区 | 地址 | 大小 | 归属 |
| --- | ---: | ---: | --- |
| `nvs` | `0x9000` | 16 KiB | 平台 SDK/校准 |
| `otadata` | `0xd000` | 8 KiB | 两扇区 OTA 选择 |
| `phy_init` | `0xf000` | 4 KiB | 平台 |
| `factory` | `0x10000` | 1.5 MiB | 常驻启动器 |
| `app0` | `0x190000` | 2 MiB | OTA 0 |
| `app1` | `0x390000` | 2 MiB | OTA 1 |
| `app2` | `0x590000` | 2 MiB | OTA 2 |
| `pos_meta` | `0x790000` | 64 KiB | 注册与启动状态 |
| `appdata0` | `0x7a0000` | 128 KiB | 位置 0 的稳定 ID |
| `appdata1` | `0x7c0000` | 128 KiB | 位置 1 的稳定 ID |
| `appdata2` | `0x7e0000` | 128 KiB | 位置 2 的稳定 ID |

末字节为 `0x7fffff`；应用 64 KiB 对齐，数据扇区对齐，不重叠，没有额外文件系统。旧 24 KiB NVS 与新 OTA 区重叠；首次迁移必须有导出/导入方案，不能直接覆盖表后承诺保留设置。

## Launcher 参考研究

获准后 Git 下载仍因网络失败，因此通过 GitHub/raw 页面检查源码；网页快照**没有绑定已验证的上游提交**，以后复用前需重新核对。没有复制 Launcher 源码。

| 文件 | 观察机制 | PassportOS 采用方式 |
| --- | --- | --- |
| [main.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/main.cpp) | 启动画面、按键退出、配置应用启动 | 新 LVGL 菜单/取消窗口；引导器不用猜 ADC 按键 |
| [app_registry.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/app_registry.cpp) | `l_apps` 按分区标签记录，扫描 OTA，启动及 BLE bonds 处理 | 完整稳定 ID、版本 blob、READY 状态、私有 NVS；不启用 BLE |
| [install_shared.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/install_shared.cpp) | 共用名称、安装元数据、分区关联和 BLE 移交 | 单一包格式与流式路径，身份不依赖名称/路径/版本 |
| [partition_install_layout.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/partition_install_layout.cpp) | 空间复用、重分区、删除候选及文件系统规划 | 固定许可写区，超大直接拒绝，不挤占他人数据 |
| [partitioner.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/partitioner.cpp) | 创建、压缩、写表、格式化及关联删除 | 无运行时重分区或全局 NVS 重置 |
| [partition_table_model.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/partition_table_model.cpp) | 表解析/校验、MD5、数据搬移、表验证、OTA 扇区选择 | 采用边界/完整性检查原则，使用固定表与 IDF OTA，不运行时改表 |
| [backup_manager.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/backup_manager.cpp) | SD `/bkp/backupData.json`、由路径生成应用编号 | 无 SD，不搬移私有数据，不用文件名或短哈希作为身份 |
| [webInterface.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/webInterface.cpp) | 鉴权路由、流式 multipart、规划与管理 | 本地按需热点/会话令牌/原始包流，不写引导器或表 |
| [nvs_helpers.cpp](https://github.com/bmorcelli/Launcher/blob/main/src/nvs_helpers.cpp) | 句柄生命周期、显式提交/错误、命名空间操作 | 具名私有分区、检查提交，不因初始化错误擦除 |

上游 [LICENSE](https://github.com/bmorcelli/Launcher/blob/main/LICENSE)为 MIT，版权 2023 shikarunochi。今后复制代码须保留相应版权与许可证，并审核各文件及依赖通知。目前仅复用保留 MIT 许可的 FoloToy 代码。

## 启动与恢复

依据 [IDF OTA](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/system/ota.html)、[引导器指南](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/bootloader.html)，并检查本地 5.5.3 的启动、OTA、RTC Kconfig 与 `rtc_custom_section` 示例。

复位后引导器先读取并清除 RTC 票据；只有软件复位、有效票据且 OTA 选择一致时才启动应用，否则进入 factory。正常上电、上次已健康且启用自动启动时提供五秒取消；超时或手动选择后，校验哈希、提交待确认启动、设置 OTA、写票据并重启。应用返回、异常、看门狗或深睡眠唤醒都重新经过无票据的菜单路径。

票据使用 SDK 预留的 RTC 区，不占用猜测的 RTC 寄存器或 GPIO。所有应用必须匹配 ABI。禁用原生回滚，因为它可能选择另一 OTA 应用，而这里要求回菜单。健康确认只控制未来自动启动，不提供签名或旧镜像副本。启动器/分区表损坏或 ROM USB 被禁用时，仍须实测硬件恢复，不能承诺对任意 Flash 损坏免疫。
