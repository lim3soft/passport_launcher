#!/usr/bin/env python3
"""Create an isolated QSO Vision source port; never edits the source checkout."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_ID = 'org.passportos.qso-vision'


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'QSO source changed: expected one occurrence of {old!r}')
    return text.replace(old, new)


def prepare(source, output):
    source, output = source.resolve(), output.resolve()
    if output.exists() or output == source or source in output.parents:
        raise ValueError('output must be a new directory outside the source checkout')
    main = (source/'main/main.c').read_text()
    wifi = (source/'main/wifi_monitor.c').read_text()
    if 'esp_wifi_set_storage(WIFI_STORAGE_RAM)' not in wifi:
        raise ValueError('Wi-Fi driver must keep SDK credentials in RAM')
    prologue = '#include "passport_sdk.h"\n#include "passport_qso_migration.h"\n'
    main = prologue + ('PASSPORT_DECLARE_APP(PASSPORT_QSO_ID);\n'
        'static const char qso_migration_marker[] __attribute__((used, retain)) = PASSPORT_QSO_MIGRATION_MARKER;\n') + main
    wifi = prologue + wifi
    main = main.replace('nvs_open(', 'passport_app_open(PASSPORT_QSO_ID, ')
    wifi = wifi.replace('nvs_open(', 'passport_app_open(PASSPORT_QSO_ID, ')
    wifi = replace_once(wifi, 'static esp_err_t migrate_legacy_profile(void)\n{',
        'static esp_err_t migrate_legacy_profile(void)\n{\n'
        '    /* A restored modern profile must not be overwritten by old keys. */\n'
        '    for(size_t i=0;i<WIFI_PROFILE_MAX;i++) {\n'
        '        char ssid[33], password[65];\n'
        '        if(read_profile(i,ssid,sizeof(ssid),password,sizeof(password))) return ESP_OK;\n'
        '    }')
    main = replace_once(main, 'nvs_flash_init(); load_settings(); bsp_i2c_init();',
        'ESP_ERROR_CHECK(passport_qso_apply_migration());\n'
        '    ESP_ERROR_CHECK(nvs_flash_init()); load_settings(); bsp_i2c_init();')
    main = replace_once(main, 'bsp_btn_t b=c&0xf; bsp_btn_ev_t e=c>>4;',
        'bsp_btn_t b=c&0xf; bsp_btn_ev_t e=c>>4;\n'
        '        if(s_options_screen && b==BSP_BTN_OK && e==BSP_BTN_DOUBLE) {\n'
        '            if(save_settings()) {\n'
        '                if(s_audio_ready)bsp_audio_set_volume(0);\n'
        '                passport_return_to_launcher();\n'
        '            }\n'
        '            continue;\n'
        '        }')
    main = replace_once(main, 'UP/DOWN select  OK open\\nLONG OK back',
        'UP/DOWN select  OK open\\n2xOK PassportOS  HOLD OK back')
    main = replace_once(main, 'ESP_LOGI(TAG,"presets=%u audio=%d",(unsigned)bm_preset_count,s_audio_ready);',
        'if(s_buttons_ready && s_screen)\n'
        '        ESP_ERROR_CHECK(passport_app_mark_healthy(PASSPORT_QSO_ID));\n'
        '    ESP_LOGI(TAG,"presets=%u audio=%d",(unsigned)bm_preset_count,s_audio_ready);')
    output.mkdir(parents=True)
    for folder in ('main', 'assets', 'managed_components'):
        if (source/folder).is_dir(): shutil.copytree(source/folder, output/folder)
    # Source checkout can use a neighboring dependency cache; copy only locally.
    if not (output/'managed_components').exists():
        shutil.copytree(ROOT/'managed_components', output/'managed_components')
        cached = source.parent/'ai-passport/managed_components/espressif__esp_websocket_client'
        if cached.exists(): shutil.copytree(cached, output/'managed_components/espressif__esp_websocket_client')
    for file in ('LICENSE', 'dependencies.lock'):
        if (source/file).exists(): shutil.copy(source/file, output/file)
    (output/'main/main.c').write_text(main)
    (output/'main/wifi_monitor.c').write_text(wifi)
    cmake = (output/'main/CMakeLists.txt').read_text().replace('REQUIRES bsp ', 'REQUIRES passport_sdk bsp ')
    (output/'main/CMakeLists.txt').write_text(cmake)
    (output/'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.16)\nset(PROJECT_VER "2.3.3-passportos.2")\n'
        f'set(EXTRA_COMPONENT_DIRS "{ROOT}/components")\n'
        'include($ENV{IDF_PATH}/tools/cmake/project.cmake)\nproject(QSO-Vision-PassportOS)\n')
    defaults = (source/'sdkconfig.defaults').read_text()
    defaults += ('\nCONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y\n'
        'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x10\n'
        'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_IN_CRC=n\n'
        'CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192\n')
    (output/'sdkconfig.defaults').write_text(defaults)
    shutil.copy(ROOT/'examples/identity-app/partitions.csv', output/'partitions.csv')
    provenance = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted((source/'main').glob('*')) if p.is_file()}
    (output/'port-provenance.json').write_text(json.dumps({
        'source': str(source), 'app_id': APP_ID, 'source_files_sha256': provenance,
        'changes': ['private NVS handles', 'RTC ABI declaration', 'Options double OK return',
                    'healthy acknowledgement', 'resumable legacy settings application before app initialization'],
        'device_tested': False}, indent=2)+'\n')
    print(output)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    try: prepare(args.source, args.output)
    except (OSError, ValueError) as error: p.exit(1, str(error)+'\n')


if __name__ == '__main__': main()
