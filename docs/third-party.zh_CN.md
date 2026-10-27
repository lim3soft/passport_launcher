<p align="right"><strong>简体中文</strong> · <a href="third-party.md">English</a></p>

# 第三方通知

复制的 FoloToy BSP（`components/bsp`）与固件校验/归档工具（改名并补充 OTA 镜像的 `tools/verify_firmware.py`、`tools/archive_firmware.py`）保留根目录 MIT `LICENSE`，版权 2026 FoloToy。QSO 代码仅在独立生成的构建目录内，保留其 LICENSE 和资源通知；发布时须保留字体/图片等许可。

仅研究而未复制 `bmorcelli/Launcher`。其[许可证](https://github.com/bmorcelli/Launcher/blob/main/LICENSE)为 MIT，版权 2023 shikarunochi。今后复制代码或实质部分须保留版权及完整许可，并检查各文件、子模块。网页研究未绑定已验证提交。

ESP-IDF 是外部工具链，具有自身[许可与第三方通知](https://github.com/espressif/esp-idf/blob/v5.5.3/LICENSE)。自定义引导钩子使用其接口和 SDK 函数，未粘贴 SDK 实现源码。分发链接固件时须保留组件通知。

托管依赖及来源通知保留于 `managed_components/`，版本与哈希见 `dependencies.lock`：LVGL 9.6.0~1、Espressif LVGL port 2.9.0、button 4.2.0、codec-dev 1.6.2、cmake utilities 1.2.0。生成的 QSO 另用 esp_websocket_client，保留其资源/字体许可。托管源码未作为 PassportOS 跟踪源码纳入；发布包仍须保留适用的 MIT、Apache 等组件通知。本阶段未发布或上传任何内容。

本地 `delivery/THIRD-PARTY-NOTICES.txt` 汇集 SDK、组件及资源许可（含 QSO 字体通知），并同时保留根 MIT 许可。

`tools/vendor/nvs_parser.py` 未经改动地复制自 ESP-IDF v5.5.3，版权 2022–2024 Espressif Systems (Shanghai) CO LTD，Apache-2.0。保留原 SPDX/版权头和完整 `tools/vendor/ESP-IDF-LICENSE`，仅用于 QSO 数据导出，不用于安装固件。

Passport Brand 22 位图子集及原 Liberation Sans Bold 源字体使用 SIL OFL 1.1，Google/Red Hat 版权和保留字体名称声明位于 `assets/fonts/passport-brand/LICENSE.txt`。子集使用自己的字体名称，完整许可已加入固件离线声明。
