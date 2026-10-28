# Passport Launcher v0.4.2

More compatible community BIN files can now be installed, including the supplied badminton score, Pokémon and 千年家族 builds. Space checks include each app's private save/resource requirements. Updates retain private data; confirmed uninstall clears every owned region. Interrupted-install retry and startup recovery checks were expanded. QSO Vision and Internet Radio remain supported; no applications are bundled.

## Choose the correct BIN

- **Existing 0.4.0/0.4.1:** power-cycle, cancel Auto start, wait at the launcher menu, then flash `Passport-Launcher-0.4.2-update.bin` at **0x0**, with **whole-chip erase OFF**. This replaces the launcher and bootloader while retaining app/private-data/metadata regions.
- **Fresh installation / old 0.1–0.3 migration:** `Passport-Launcher-0.4.2-fresh.bin` at **0x0**. It **clears all existing apps and settings**. The community one-click firmware uses this full fresh image.
- Reload the portal after upgrading. Choose Install a ROM, join **Passport Launcher** / **aipassport**, and open **http://192.168.168.1**. Upload the original compatible local BIN, wait for completion, then select it on the device.
- Neither launcher BIN belongs in its own app-upload portal. Do not retain extended-layout entries when downgrading the launcher.

The three supplied apps need more storage in total than the shared pool provides; they cannot all be installed together. Unusual raw-address/OTA/recovery assumptions may still require a source port. See the compatibility guide and exact input hashes in the source.

Build and host tests pass, including all three real input byte streams, private-data updates/uninstall, variable extents, manifest corruption and I/O interruptions. The owner reports the fix is working on their device. This host did not execute ESP32 code or independently validate RF, stack, physical flash wear or electrical power cuts.

Developed by **9W3SPY**. MIT application license; dependencies retain their own licenses. Full notices are included. Inspired by M5 Launcher. The cover is a labeled illustration; UI assets in the repository are host renders with sample app names.

Community project: [1704](https://ai-passport.folotoy.cn/plays/1704/). Source: [lim3soft/passport_launcher](https://github.com/lim3soft/passport_launcher).

## 中文更新说明

修复更多社区 BIN 无法安装的问题，支持本次提供的羽毛球计分、Pokémon 和千年家族等兼容文件。容量检查包括存档与资源需求，更新保留私有数据，确认卸载清除全部占用，并完善安装中断重试与启动恢复。继续兼容 QSO Vision 和 Internet Radio，没有内置任何第三方应用。

已有 0.4.0/0.4.1 请先返回启动器菜单，再将短 **update.bin 写入 0x0，关闭整片擦除**。**fresh.bin 会清除全部应用和设置**；社区一键安装使用 fresh 镜像。升级后重新加载网页。三款新应用合计超过可用空间，不能同时全部保留。完整编译与主机检查通过，作者反馈实机可正常使用；主机没有独立执行实机验收。开发 9W3SPY，应用 MIT，保留依赖许可，灵感来自 M5 Launcher。
