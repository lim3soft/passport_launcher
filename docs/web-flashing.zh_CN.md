<p align="right"><strong>简体中文</strong> · <a href="web-flashing.md">English</a></p>

# 浏览器全新安装 — PassportOS 0.1.2

用户已选择全新安装，不需要备份或导出现有应用配置。使用 Windows 笔记本与网页刷写工具即可；此前的 Python 采集和迁移流程不再是本次安装的前提。

下载 `delivery/PassportOS-0.1.2-fresh-8MB.bin`，它是面向已审计 ESP32-C3 / 8 MiB 配置的 **8,388,608 字节首次安装镜像**，起始地址为 **0x0**。写入范围为 `0x000000` 至 `0x7fffff`，会替换所有现有固件和设置。需要保留设置时，不能将它用于日常切换或更新。

镜像完整保留已校验的 1,406,208 字节合并固件，并在其后补充 `0xff` 至 Flash 末尾，以初始化未来应用槽、注册表和私有数据区。仅写入较小合并镜像不会清空这些区域。镜像不修改 eFuse；完整写入本文件时无需另行擦除。

## 安装

1. 在 Windows 笔记本的 Chrome 或 Edge 中打开[官方 AI-Passport 网页刷写工具](https://ai-passport.folotoy.cn/tools/web-flasher/)，选择**安装本地固件**。[官方开发指南](https://ai-passport.folotoy.cn/en/guides/create-your-own-play/)介绍了本地固件安装方式。
2. 将 AI-Passport 开机，用可传输数据的 USB 线连接，并关闭其他串口监视器或刷写程序。在浏览器设备选择框中选择此设备的端口。
3. 选择 `PassportOS-0.1.2-fresh-8MB.bin`。若显示地址字段，设置为 **0x0**。目标为 **ESP32-C3、8 MB**；检测结果不符时停止。本主机未实测芯片修订、容量或安全状态，不能强制绕过芯片、容量、修订或安全错误。
4. 完整安装，等待成功与校验完成，全程保持供电和 USB 连接。工具没有自动重启时，完成后重新开机。
5. 确认出现 PassportOS 菜单，上/下/OK 正常，已安装应用位置为空。先记录结果，再安装应用。

失败时保留并反馈错误，不要猜测接线或 BOOT 操作，也不要更改 eFuse。主机编译成功不等于实机验收。

## 随后安装应用

`delivery/` 中的 `counter-a.passport`、`counter-b.passport` 与 `qso-vision.passport` 是独立应用包。在 PassportOS 选择 **Install New App**，用手机连接屏幕显示的 Wi-Fi 热点，访问 `http://192.168.4.1` 并上传应用包。不能把这些包作为 USB 刷机镜像。先在两个计数器保存不同数值，切换并检查保留结果。QSO Vision 可选安装，并未内置于启动器。

## 校验

全新安装镜像 SHA-256：`259c592d3beca37db0100452a80710dbe6ba0fddf33998d433d9ba0517cee7d5`。

对应调试归档位于 `build/firmware/259c592d3beca37db0100452a80710dbe6ba0fddf33998d433d9ba0517cee7d5/`。归档和 ELF 身份、分区布局、原镜像字节及空白数据区域均通过主机检查；底层 0.1.2 固件编译和主机测试已通过。**Windows 刷写与实机测试：未执行。** 本主机没有写入设备。详见[安装校验证据](fresh-install-results.json)。
