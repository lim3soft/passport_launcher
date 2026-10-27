<p align="right"><a href="third-party.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Third-party notices

Copied FoloToy AI-Passport BSP (`components/bsp`) and firmware verification/archive tools (`tools/verify_firmware.py`, `tools/archive_firmware.py`, adapted for the new name and OTA image) are covered by the retained root `LICENSE`, MIT, copyright 2026 FoloToy. QSO code remains in a separately generated build directory, with its own copied LICENSE and asset notices. Do not publish that copy without retaining font/image/license notices.

`bmorcelli/Launcher` was studied, not copied. Its repository [license](https://github.com/bmorcelli/Launcher/blob/main/LICENSE) is MIT, copyright 2023 shikarunochi. Any future copied source/substantial portions require retaining that notice and license text. Check individual files/submodules as well as the root license before reuse. Upstream web review was not pinned to a verified commit.

ESP-IDF is an external toolchain, with its own [license and third-party notices](https://github.com/espressif/esp-idf/blob/v5.5.3/LICENSE). The custom hook uses its public hook mechanism and SDK functions; SDK implementation source was not pasted into PassportOS. Preserve SDK component licenses when distributing linked firmware.

Managed dependencies and their source notices remain in `managed_components/`, with versions/hashes in `dependencies.lock`: LVGL 9.6.0~1, Espressif LVGL port 2.9.0, button 4.2.0, codec-dev 1.6.2, and cmake utilities 1.2.0. The generated QSO port also uses esp_websocket_client and its retained asset/font licenses. Dependency source is not included as tracked PassportOS source; release packaging must retain applicable MIT, Apache and other component notices. Nothing has been published or uploaded in this increment.

The local `delivery/THIRD-PARTY-NOTICES.txt` retains collected SDK/component/asset license files, including the QSO font notice, alongside the root MIT license.

The unchanged `tools/vendor/nvs_parser.py` is copied from ESP-IDF v5.5.3, copyright 2022–2024 Espressif Systems (Shanghai) CO LTD, Apache-2.0. Its original SPDX/copyright header and full `tools/vendor/ESP-IDF-LICENSE` are retained. It is used for data-only QSO export, not firmware image installation.

The Passport Brand 22 bitmap subset and its original Liberation Sans Bold source are under SIL OFL 1.1, with retained Google/Red Hat copyright and reserved-font-name notices in `assets/fonts/passport-brand/LICENSE.txt`. The subset uses its own font name. Full notices are included in the offline firmware license bundle.
