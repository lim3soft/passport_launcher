<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# Passport Launcher 0.4.1

<p align="center"><img src="assets/community/0.4.1/cover.png" width="420" alt="Passport Launcher illustrated cover" /></p>


[下载 v0.4.1](https://github.com/lim3soft/passport_launcher/releases/tag/v0.4.1) · [社区投稿（待审核）](https://ai-passport.folotoy.cn/plays/1704/) · [界面预览](assets/community/0.4.1/)

**首次安装：** 从 Releases 下载 `Passport-Launcher-0.4.1-fresh.bin` 并写入 **0x0**，会明确清除已有应用和设置。**已有 0.4.0：** 使用较小的 update BIN，关闭整片擦除，并按下方升级步骤操作。封面为示意图，界面截图来自主机渲染。


由 **9W3SPY** 开发，面向已核实的 FoloToy AI-Passport ESP32-C3 / 8 MiB 主板。QSO Vision 仍是独立应用，没有内置在启动器固件中。原 AI-Passport 与 QSO Vision 源码工作目录保持不变。

本版移除社区下载器，将三个固定 2 MiB 应用位置改为共享 Flash 的可变分配。通过网页上传本地 **`.bin`**，然后在设备上选择运行。网页不接受专用应用包格式。切换已安装应用无需互联网。

共享应用空间为 **6.375 MiB**，每个应用另预留 **128 KiB 私有设置空间**及 Flash 对齐开销。代码按实际镜像大小向上取整至擦除扇区，起始地址按 64 KiB 对齐。设备和网页显示剩余空间；网页另显示扣除新应用设置预留后当前可容纳的最大 ROM。取消每应用 2 MiB 上限；实际数量由镜像、设置和 16 项应用索引共同决定。存储有限，不兼容芯片、SDK 或自定义分区的镜像仍被拒绝。

## 从 0.4.0 升级

使用 **Passport-Launcher-0.4.1-update.bin 写入 0x0**，并**关闭整片擦除**。先断电重启、取消自动运行，等待启动器菜单，以保存上次社区应用的默认 NVS 快照。升级镜像在共享应用区之前结束，分区表、SDK 2 注册表和私有数据布局不变，应用与已保存快照均在写入范围之外。短合并镜像仍会重置平台/默认 NVS，因此必须先返回菜单。

0.4.1 使用更大、加粗的 Passport Launcher 主菜单标题，以分隔线区分应用和系统操作；开发者署名只放在 About。自动启动默认“上次启动应用”，也可固定选择已安装应用，保留独立开关与可取消五秒倒计时。网页显示真实上传进度、等待设备校验后才报告完成/错误，移除 QSO 旧设置导入说明段，并在 About 与底部署名处加入开发者 GitHub 链接。

从旧 0.1–0.3 固定槽迁移仍需明确全新安装并清除旧应用/设置。构建另生成填充至 8 MiB 的 `build/PassportOS-fresh.bin`，不可用于普通 0.4.0 保留数据升级。此主机未刷写设备。

启动后选择 **Install a ROM**。连接 Wi-Fi **Passport Launcher**，密码 **aipassport**，访问 **http://192.168.168.1**。选择本地 `.bin`，等待检查后点击 **Install / update**。在设备上长按 OK 返回应用库并选择运行。标准合并 BIN 只提取应用载荷，不安装其引导程序或分区表。

保留整行圆角高亮、长标题滚动、默认 Cancel 且卸载会删除全部保存数据的确认、自动启动开关及高亮选定启动应用名和可取消五秒倒计时，以及设备和网页 About。About 显示版本、ESP-IDF 5.5.3、MIT/开源许可及 **Inspired by M5 Launcher**；完整许可可离线访问 `/licenses.txt`。

## 兼容与存储

提供的 QSO Vision 与 Internet Radio BIN 通过导入和生产管理器主机模型，但未执行其中的 ESP32 机器码。Pokémon 和应用 1434 的代码大小可放入空共享区，但其额外资源/数据分区仍需源码适配。取消容量限制不会自动提供这些分区。

普通社区 ESP-IDF 5.5.x 应用在私有空间内分别保存 16 KiB 默认 NVS 快照。原生应用必须使用本目录 **SDK 2** 并预留 32 字节自定义 RTC 内存；旧 SDK 1 应用需重新编译或改用其原始社区 BIN。原生应用保留稳定 ID 的私有 NVS 访问和健康确认。启动器不附带 QSO 应用。

更新保留私有数据，可移动当前应用代码，不移动其他应用。中断的安装不可启动，可重试。碎片可能造成剩余字节总数足够但无法放入 ROM，应参考最大 ROM 数值。卸载成功擦除代码和数据后释放空间。详见[共享存储与恢复](docs/shared-storage.zh_CN.md)及[界面指南](docs/interface.zh_CN.md)。

应用复位均返回启动器，冷启动自动运行可取消。引导器先重建再读取分区表，可恢复中断的表写入。启动应用及返回启动器分别可能擦除一次表扇区，会增加磨损，尚未做寿命测试。保护与隔离依赖应用合作，并非硬件沙箱。完整性哈希不认证发布者。

## 验证与开发

激活 ESP-IDF **5.5.3** 后运行 `bash tools/validate.sh`，执行主机检查、隔离默认配置构建、合并升级镜像及独立全新安装镜像校验，并将匹配 ELF/MAP 保存在 `build/firmware/<sha256>/`。主机检查需要 C、Python 3、Node.js 和 OpenSSL 开发库。`bash tools/render_ui_preview.sh` 使用实际锁定的 LVGL 和 48 KiB 内存池；预览来自主机渲染，不是实机截图。

[验证记录](docs/validation-results.json)分别记录编译、主机测试和设备验收。实机 RF/堆/栈、应用执行、视觉时序、Flash 寿命和实际断电恢复仍待设备测试。旧固定槽架构和 SDK 1 交付归档只作历史参考。

[硬件审计](docs/research.zh_CN.md)列出源码证据与待测事项。保留 [MIT 许可](LICENSE)与[第三方声明](docs/third-party.zh_CN.md)。M5 Launcher 仅为设计灵感，未复制其源代码。
