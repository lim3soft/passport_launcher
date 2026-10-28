<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# Passport Launcher 0.4.2

<p align="center"><img src="assets/community/0.4.1/cover.png" width="420" alt="Passport Launcher illustrated cover" /></p>


[已发布版本](https://github.com/lim3soft/passport_launcher/releases) · [AI-Passport 社区](https://ai-passport.folotoy.cn/plays/1704/) · [界面预览](assets/community/0.4.1/)

**0.4.2 兼容性更新：** 支持不同的应用起始偏移、更大 NVS，以及附加私有数据、资源和 test/recovery 分区。新提供的三个社区 BIN 均通过生产主机模型测试。详见[测试结果与升级步骤](docs/community-compatibility-0.4.2.zh_CN.md)。已有 0.4.0/0.4.1 请将短升级镜像写入 **0x0** 并**关闭整片擦除**；独立 fresh 镜像会清除应用和设置。封面为示意图，界面截图来自主机渲染。


由 **9W3SPY** 开发，面向已核实的 FoloToy AI-Passport ESP32-C3 / 8 MiB 主板。QSO Vision 仍是独立应用，没有内置在启动器固件中。原 AI-Passport 与 QSO Vision 源码工作目录保持不变。

本版移除社区下载器，将三个固定 2 MiB 应用位置改为共享 Flash 的可变分配。通过网页上传本地 **`.bin`**，然后在设备上选择运行。网页不接受专用应用包格式。切换已安装应用无需互联网。

共享应用空间为 **6.375 MiB**，每个应用至少预留 **128 KiB 私有设置空间**，另计导入分区和 Flash 对齐开销。代码按实际镜像大小向上取整至擦除扇区，起始地址按 64 KiB 对齐。设备和网页显示剩余空间；网页另显示扣除新应用设置预留后当前可容纳的最大 ROM。取消每应用 2 MiB 上限；实际数量由镜像、设置和 16 项应用索引共同决定。存储有限，不兼容芯片、SDK 或未支持分区布局的镜像仍被拒绝。

## 从 0.4.0 / 0.4.1 升级

使用 **Passport-Launcher-0.4.2-update.bin 写入 0x0**，并**关闭整片擦除**。先断电重启、取消自动运行，等待启动器菜单，以保存上次社区应用的默认 NVS 快照。升级镜像在共享应用区之前结束，保留永久分区表与已有注册项，引导程序和模型增加私有分区映射扩展，应用与已保存快照均在写入范围之外。短合并镜像仍会重置平台/默认 NVS，因此必须先返回菜单。

0.4.1 使用更大、加粗的 Passport Launcher 主菜单标题，以分隔线区分应用和系统操作；开发者署名只放在 About。自动启动默认“上次启动应用”，也可固定选择已安装应用，保留独立开关与可取消五秒倒计时。网页显示真实上传进度、等待设备校验后才报告完成/错误，移除 QSO 旧设置导入说明段，并在 About 与底部署名处加入开发者 GitHub 链接。

从旧 0.1–0.3 固定槽迁移仍需明确全新安装并清除旧应用/设置。构建另生成填充至 8 MiB 的 `build/PassportOS-fresh.bin`，不可用于普通 0.4.0 保留数据升级。此主机未刷写设备。

启动后选择 **Install a ROM**。连接 Wi-Fi **Passport Launcher**，密码 **aipassport**，访问 **http://192.168.168.1**。选择本地 `.bin`，等待检查后点击 **Install / update**。在设备上长按 OK 返回应用库并选择运行。合并 BIN 导入应用及受支持私有分区，不安装上传文件的引导程序，由常驻引导程序生成运行时分区映射。

保留整行圆角高亮、长标题滚动、默认 Cancel 且卸载会删除全部保存数据的确认、自动启动开关及高亮选定启动应用名和可取消五秒倒计时，以及设备和网页 About。About 显示版本、ESP-IDF 5.5.3、MIT/开源许可及 **Inspired by M5 Launcher**；完整许可可离线访问 `/licenses.txt`。

## 兼容性与存储

QSO Vision 和 Internet Radio 继续支持。新提供的羽毛球计分、Pokémon 和千年家族 BIN 已通过主机上的真实字节导入、流式安装、启动分区映射、私有数据保留及卸载测试。没有连接设备或执行 ESP32 代码，仍需实机验证。三个新应用的最低合计占用超过共享池，不能同时全部装下；详见[兼容性说明](docs/community-compatibility-0.4.2.zh_CN.md)。

应用名不受白名单限制。原始 app-only/标准合并布局继续使用独立 16 KiB 默认 NVS 快照；其他受支持合并布局按其声明大小分配 NVS、资源与恢复区。更新保留存档，分区布局改变时需要先卸载；确认卸载会清除全部私有数据。空间检查包含附加分区，碎片可能限制可安装大小。

原生应用使用本仓库 SDK 2 和 32 字节自定义 RTC 保留区。旧 SDK 2 应用与扩展注册项混用时应使用当前 core/model 重编译；装入新布局后不要保留注册表降级启动器。SDK 1 应用需要重编译。未内置任何 QSO 应用。

重启返回启动器，冷启动自动运行仍可取消。引导程序先重建分区表，恢复中断的写表操作。切换会增加分区表扇区擦写；实机耐久与电气掉电尚未验证。此为协作式隔离，并非硬件沙箱；使用原始绝对地址、独立 OTA 或自刷引导程序的应用仍可能需要源码适配。

## 验证与开发

激活 ESP-IDF **5.5.3** 后运行 `bash tools/validate.sh`，执行主机检查、隔离默认配置构建、合并升级镜像及独立全新安装镜像校验，并将匹配 ELF/MAP 保存在 `build/firmware/<sha256>/`。主机检查需要 C、Python 3、Node.js 和 OpenSSL 开发库。`bash tools/render_ui_preview.sh` 使用实际锁定的 LVGL 和 48 KiB 内存池；预览来自主机渲染，不是实机截图。

[验证记录](docs/validation-results.json)分别记录编译、主机测试和设备验收。实机 RF/堆/栈、应用执行、视觉时序、Flash 寿命和实际断电恢复仍待设备测试。旧固定槽架构和 SDK 1 交付归档只作历史参考。

[硬件审计](docs/research.zh_CN.md)列出源码证据与待测事项。保留 [MIT 许可](LICENSE)与[第三方声明](docs/third-party.zh_CN.md)。M5 Launcher 仅为设计灵感，未复制其源代码。
