# Passport Launcher v0.4.1

A multi-app organizer by **9W3SPY**. QSO Vision and other applications are installed separately, not bundled.

## Downloads

- **First installation / migration from the old fixed-slot launcher:** `Passport-Launcher-0.4.1-fresh.bin` at **0x0**. This full image **clears existing applications and settings**.
- **Update an existing 0.4.0 shared-storage launcher:** `Passport-Launcher-0.4.1-update.bin` at **0x0**, with **whole-chip erase OFF**. First power-cycle, cancel Auto start and wait for the launcher menu so the running community app's settings are captured. The update preserves app/private-data/metadata extents; the merged prefix resets platform/default NVS.
- SHA256SUMS.txt and manifest.json identify the exact files. Do not upload either launcher image into the launcher app-install portal.

## Included in this version

Auto start defaults to the last-booted app or can pin another installed app, with independent On/Off and a cancellable five-second countdown. The main menu has bold Passport Launcher branding and an app/action divider; developer credit is inside About. The portal adds actual upload progress and confirmed installation completion, removes the legacy-settings advice paragraph, and includes developer GitHub links and footer credit.

Upload compatible local `.bin` apps from **Install a ROM** using Wi-Fi **Passport Launcher**, password **aipassport**, at **http://192.168.168.1**. Shared-space checks, per-app settings, confirmed full uninstall and offline switching remain available. Extra resource/custom-layout applications may need adaptation.

## Validation and licensing

Build and host checks pass: ten C sanitizer executables, 24 Python tests, browser progress/error models and the native UI renderer. **Physical device tests are not run** for this build; actual app execution, RF/heap/stack and electrical power-cut recovery remain unverified. Cover is an illustration; screenshots are host renders with example app names, not preinstalled applications or device photographs.

Application MIT license; dependencies and the brand font retain their own licenses. Full notices are included. Inspired by M5 Launcher.

Community project **1704**, revision **3933**: https://ai-passport.folotoy.cn/plays/1704/

---

# Passport Launcher v0.4.1 中文说明

由 **9W3SPY** 开发的多应用管理工具，QSO Vision 等应用需分别安装，未内置。

- **首次安装／旧固定槽迁移：** fresh.bin 写入 **0x0**，将清除原有应用与设置。
- **已有 0.4.0 升级：** update.bin 写入 **0x0**，**关闭整片擦除**。先断电重启、取消自动运行并等待启动器菜单保存上次社区应用设置；应用、私有数据与元数据在写入范围之外，合并前缀仍会重置平台/默认 NVS。
- 自动启动可跟随上次应用或固定应用，保留独立开关和可取消倒计时。主菜单增加加粗品牌与分隔线，开发者仅在 About 内显示。
- 网页增加真实上传进度及最终完成确认，移除旧设置导入说明段，增加 GitHub 与底部署名。
- 选 Install a ROM，用手机或电脑连热点 **Passport Launcher**（密码 **aipassport**），访问 **http://192.168.168.1** 上传兼容本地 `.bin`。
- 编译／主机检查通过；**本版未做实机测试**。封面为示意图，界面为主机渲染，演示应用未预装。应用 MIT，依赖和字体保留各自许可，完整声明随文件提供。
- 社区项目 **1704**／版本记录 **3933**。
