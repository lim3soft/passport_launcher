<p align="right"><strong>简体中文</strong> · <a href="application-contract.md">English</a></p>

# 应用契约，SDK ABI 2

Passport Launcher 0.4.0 使用共享存储注册表、SDK ABI 2 与 32 字节一次性 RTC 票据。SDK 1 二进制依赖旧地址/注册表，必须重新编译。社区 BIN 仍是独立导入模式，使用默认 NVS 快照。网页只接受 `.bin`；内部包头和 Python 打包工具仅用于开发传输/测试。

## 身份与私有存储

稳定 ID 长 1–63 个 ASCII 字符，以小写 `a-z` 开头，后续为 `a-z0-9._-`。名称和版本可变化而 ID 不变。使用一次 `PASSPORT_DECLARE_APP("org.example.app")` 保留 `PASSPORTOS_ABI2:` 声明。这是兼容声明，不是发布者签名。

`passport_app_open(PASSPORT_APP_ID, namespace, mode, &handle)` 读取 v2 注册表，匹配 READY 身份并核对运行镜像地址/容量。临时启动表始终把选中应用作为 OTA 0，不能根据 OTA 子类型推断身份/存储。辅助 API 只打开该记录的私有 NVS，更新代码时数据地址稳定。应用可安全重复使用命名空间/键名。每个私有分配区为 128 KiB，包含 NVS 开销，不保证有等量有效记录容量。

Wi-Fi 配置、进度、偏好和应用校准应存于私有 NVS，Wi-Fi 驱动使用 `WIFI_STORAGE_RAM`。普通 `nvs_open` 访问共享平台 NVS，不是原生私有接口。社区模式另在运行前后保存/恢复 16 KiB 默认 NVS，原生 SDK 2 应用不得绕过私有 API。BLE 持久化、文件系统或资源分区需另外审计集成。

不得写物理分区表、共享元数据、其他应用或绝对 Flash 地址。不要在启动器下运行应用自带的整机更新器，应从启动器网页更新应用。所有固件仍有原生权限，隔离依赖合作而非硬件沙箱。除 SDK 健康确认外，仅启动器修改注册表。

## 构建与生命周期

使用已核实的 ESP32-C3、DIO、80 MHz、8 MiB 和 ESP-IDF 5.5.3，保留现有 BSP 初始化及屏幕/按键驱动。包含当前 `passport_core` / `passport_sdk`，启用 `CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y`、`CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x20`，自定义 RTC 不参与保留区 CRC。应用构建时 CSV 不是设备实际布局；运行表由启动器引导程序提供。

关键初始化完成后才调用 `passport_app_mark_healthy(PASSPORT_APP_ID)`，清除待确认启动状态并允许后续可选冷启动自动运行。返回前保存设置、关闭句柄、停止任务/音频/网络，再调用 `passport_return_to_launcher()`。普通复位没有新票据，引导器重建并选择 factory。社区应用可断电重启或普通复位返回；冷启动自动运行可取消。

安装器检查完整性、芯片/Flash/SDK、身份和分配后才标记 READY，不自动运行上传的镜像。新私有区先初始化，更新保留已初始化数据。更新失败会使当前应用等待修复，不擦除其他应用/设置。完整卸载在代码与数据擦除完成后才释放所有权。

## 空间与兼容性

精确地址、分配、表恢复和磨损影响见[共享存储](shared-storage.zh_CN.md)。镜像大小取决于实际可用连续区域、私有数据预留、对齐及镜像内存要求。共享池 6.375 MiB，索引最多 16 个身份，不再固定每应用 2 MiB，也不承诺无限数量的完整固件。

支持的标准合并社区 BIN 在本地提取，仅传输已校验的应用。额外资源/文件系统/数据分区会被拒绝，不会静默忽略。扩大代码区不代表自定义 Pokémon / 应用 1434 已兼容。原始社区 QSO/Radio BIN 通过主机测试；旧 SDK 1 的启动器适配 QSO 包需按 SDK 2 重建，未附带在启动器内。

主机使用生产 SDK/管理器和持久化字节/NVS 后端验证第四应用身份及 2.5 MiB 镜像，不等于 ESP32 执行或 RF/堆/断电/寿命测量。正式验收前仍需实机验证 SDK 和各应用。
