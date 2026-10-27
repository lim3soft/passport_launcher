<p align="right"><strong>简体中文</strong> · <a href="migration.md">English</a></p>

# QSO 设置迁移：PassportOS 0.1.1

仅数据导出/导入实现已通过主机测试。本环境尚未读取、恢复或验证真实设备设置。此流程仅适用于已审计的 QSO Vision 2.3.3 发布版本及单应用布局，实机迁移仍须确认设备、审核计划并明确批准刷写。

## 改分区前导出

旧 NVS 地址 `0x9000`、长度 `0x6000`；PassportOS 将其中最后两个扇区用作 OTA 选择数据。**任何擦除或固件写入前，先读取旧分区表和 NVS**。无需备份完整固件。以下是供审核后手动执行的读取示例，并未实际执行：

```bash
# 激活 ESP-IDF 5.5.3，先确认正确设备/端口。
mkdir -p private
umask 077  # POSIX shell：保护读取文件
python -m esptool --chip esp32c3 --port PORT read_flash 0x8000 0x1000 private/legacy-table.bin
python -m esptool --chip esp32c3 --port PORT read_flash 0x10000 0x120 private/legacy-app-header.bin
python -m esptool --chip esp32c3 --port PORT read_flash 0x9000 0x6000 private/legacy-nvs.bin
python3 tools/qso_settings.py export private/legacy-nvs.bin --output private/qso.qso-settings
python3 tools/qso_settings.py verify private/qso.qso-settings
```

`qso_settings.py` 不打开串口、不写设备；复用未改动的 Espressif 5.5.3 NVS 解析器，检查页面/条目/字符串 CRC、状态及版本，只导出已审计的 `monitor` 键。重复有效键、未知 QSO 键、无效值、过渡/损坏页面、不支持的加密或截断读取均拒绝，不猜测或丢弃。空或缺失命名空间没有支持的非空导出，应另行审核。

导出保留存在的通话组、音量、调光、亮度、最多八组 Wi-Fi、旧 Wi-Fi 键及最后 SSID；不导入 SDK 其他命名空间或 RAM 历史。保留 Wi-Fi 原始字节，不假设 UTF-8。终端只显示数量/哈希，不打印 SSID/密码。POSIX 下输出权限 `0600`，拒绝覆盖已有文件。原始 NVS 和导出都含凭据，须私密保存，不纳入源码；本项目忽略 `private/` 与 `*.qso-settings`。

## 生成可审核计划

先完成启动器校验构建：

```bash
python3 tools/plan_qso_migration.py --table private/legacy-table.bin \
  --nvs private/legacy-nvs.bin --app-header private/legacy-app-header.bin \
  --settings private/qso.qso-settings \
  --firmware build/PassportOS-full.bin --port PORT --output private/migration-plan.json
```

工具只读取本地文件、生成私有 JSON，核对 288 字节应用头/描述符与已审计版本的 ELF 身份、旧三分区布局、表 MD5/标志、导出与 NVS 完全匹配，并绑定已验证的固件/调试归档。不同固件描述符/布局或不匹配导出会拒绝。少量描述符读取只检查构建身份，并非完整镜像认证，不要求读回整个固件。生成计划不代表授权，也不会执行列出的操作。

首次配置须初始化 `0x190000` 起、长度 `0x670000` 的将来应用/数据区，因为旧 QSO 可能在第一个新槽中留下代码。此操作会破坏**全部将来应用/私有/元数据区**，仅适用于审核后的单应用转换，不能用于已装 PassportOS 的设备。计划同时说明合并镜像对旧 NVS 的影响，不使用全片擦除或修改 eFuse。只有核对范围、保留导出并取得许可后，才可实际写入。

## 手机恢复

1. 新启动器中安装当前支持迁移的 QSO 包，ID 为 `org.passportos.qso-vision`，本阶段版本 `2.3.3-passportos.2`。
2. 使用 QSO 或保存新设置前，进入 **Install New App**，手机连接屏幕热点并访问 `http://192.168.4.1`。
3. 在 QSO 卡片选择 `.qso-settings`，点击 **Import legacy settings**。服务校验格式、SHA 与应用迁移标记，仅将单个 blob 暂存到 QSO 私有区。
4. 已有私有记录或不同暂存文件时拒绝导入；同文件可安全重试，包括写入/提交返回错误后。不擦除其他应用，也不通过清空现有 QSO 来导入。
5. 退出 Install 并启动 QSO。适配器先恢复数据，再读取设置、启动网络/UI 任务。失败后重启回启动器，下次启动根据不可变暂存文件重试，应用初始化不会使用部分导入的状态。
6. 全部设置写入及提交成功后，才保存完成摘要。以后启动或重传同文件不会覆盖完成后的新改动。现代 Wi-Fi 档案优先于冗余旧键。
7. 实机核对设置与保存档案，验收前保留原始读取/导出。导入未完成时，不降级到不支持迁移的适配器。

## 格式与限制

文件固定 **1056 字节**。前 1024 字节为小端、零填充载荷：`PPTQSO1\0`、版本 1、23 位存在掩码、四个有符号数值、八组 SSID/密码（各 33/65 字节）、旧 SSID/密码与最后 SSID（33/65/33）、源 NVS 的 32 字节 SHA、45 字节全零预留。最后 32 字节是整个载荷的 SHA-256。值域来自已检查的 QSO 读取逻辑；不是可执行镜像，也未加密或做发布者签名。

暂存使用私有命名空间 `pos_migrate` 的 `qso_v1` blob，完成摘要为 `applied_v1`。SDK 具名分区 API 避开共享 NVS，并拒绝偏离固定布局的运行应用或私有分区映射。[Espressif NVS 参考](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/storage/nvs_flash.html)说明其持久化语义。这里通过控制应用继续初始化的时机使多键恢复可重试，不宣称 NVS 多键事务原子性。

主机检查包括 SDK 生成的 24 KiB 合成数据、Python/C 格式一致、导出器真实 SHA/CRC、七个暂存和 31 个应用 I/O 边界的写前/写后故障、拒绝已有数据、重试及完成后保护。C 故障环境的 NVS/SHA 使用桩；真实掉电、NVS 恢复和设置显示仍待实机验收。
