<p align="right"><a href="lifecycle.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Lifecycle verification, 0.1.2

The host lifecycle test links the **production app manager, SDK and boot hook together**. A persistent in-memory backend represents flash, app values, metadata and boot selection. SHA-256 uses OpenSSL rather than a constant-output stub. This verifies control flow and ownership across simulated resets; it does not execute ESP32 application machine code or establish electrical power-loss recovery.

## Scenarios checked

| Operation | Verified in the host test |
| --- | --- |
| First install | Validated package becomes READY only after its image is complete; initial boot is the launcher |
| Independent apps | Two identities use the same `counter/value` key and retain different values through the real private-storage SDK |
| Launch and return | Manager persists pending state, sets selection and issues a ticket; production boot hook consumes it; SDK return reaches factory without another ticket |
| Health/auto start | Unacknowledged app stays pending; healthy acknowledgement permits configured clean-power-on auto start; ordinary return is a menu boot |
| Capacity | All three positions occupied rejects a fourth app; an ordinary removal still retains an ID reservation |
| Removal/reinstall | Same ID reuses its position and settings after code-only removal |
| Update | New image for the same ID preserves private values; a marker crossing a 4 KiB flash-read boundary is recognized |
| Delete data/reassign | Explicit delete erases the chosen private values before another ID can use that position; another app keeps its values |
| Corruption | A changed image byte is rejected by real SHA before launch selection changes |
| Metadata loss | Missing registry with residual app/private storage enters recovery without clearing those values |
| Uncertain writes | Every update, delete and launch I/O boundary fails before or after persistent mutation; reload and retry preserve ownership |
| Immediate retry | Retry after each failed update also succeeds through the same manager instance, without requiring a reboot first |

The current small fixtures exercise **106 update**, **8 deletion**, and **7 launch** I/O boundaries in both before-write and after-write modes. Launch errors do not issue a ticket even if OTA selection persisted, so a subsequent software reset stays in the launcher. Other apps' values and the simulated resident launcher region remain intact. Separate tests cover migration, package parsing, SDK partition bounds and the browser.

The backend treats complete NVS records as persistent and injects failures around API operations. It does not reproduce physical torn programming, SDK NVS page garbage collection, cryptographic publisher authentication, radio, display or chip revision checks. Those remain separate firmware/device acceptance items. The tiny protocol fixtures are **not executable demo apps and must never be installed on a device**. They are generated into temporary directories for host tests and are excluded from the delivery package. The separately compiled counter and QSO packages remain the actual hardware-test candidates.

## Auto-start input correction

Previously the menu could decide to launch at its five-second deadline before checking already queued input. The new startup controller gives pending input priority, including when the UI task first observes it after that deadline. Button callbacks attach monotonic event timestamps; cancellation uses those timestamps when draining the queue, so a delayed CLICK from the cancellation gesture cannot be mistaken for a new manual launch merely because graphics or other work delayed the task.

Focused tests check input at the deadline, delayed task execution, complete press/release/CLICK suppression and one automatic launch. Real switch debounce and near-deadline button response still require the board. Also, if closing the installer fails, the UI remains on that page so cleanup can be retried rather than presenting the main menu with incomplete service shutdown.

## Build and app separation

Host checks require a C compiler, Python 3, Node.js, and OpenSSL development headers/library (`libcrypto`). Firmware continues to use ESP-IDF's mbedTLS; OpenSSL is only linked to the host test. Run the normal `bash tools/validate.sh` gate.

Version 0.1.2 changes startup/input handling and tests. Its partition table, registry schema, package ABI and private-data layout are unchanged. Previously verified compatible app packages are retained with their matching ELF identities. QSO Vision is still a separately installed test/reference application: the launcher build contains its settings-import support, not its radio/UI application sources. No hardware has been flashed by this increment.
