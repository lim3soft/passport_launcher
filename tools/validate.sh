#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mode="${1:---all}"
if [[ "$mode" != --all && "$mode" != --static && "$mode" != --firmware ]]; then
    echo 'Usage: tools/validate.sh [--all|--static|--firmware]' >&2
    exit 2
fi
if [[ "$mode" != --firmware ]]; then
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Icomponents/passport_core/include tests/test_model.c \
        components/passport_core/passport_model.c -o /tmp/passportos-test-model
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-model
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Icomponents/passport_core/include tests/test_allocator.c \
        components/passport_core/passport_model.c -o /tmp/passportos-test-allocator
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-allocator
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Icomponents/passport_core/include tests/test_startup.c \
        components/passport_core/passport_startup.c -o /tmp/passportos-test-startup
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-startup
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Icomponents/passport_core/include tests/test_menu.c \
        components/passport_core/passport_menu.c components/passport_core/passport_model.c -o /tmp/passportos-test-menu
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-menu
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Itests/manager_stubs -Imain -Icomponents/passport_core/include \
        -Icomponents/passport_sdk/include tests/test_manager.c \
        components/passport_core/passport_model.c components/passport_core/passport_qso_data.c -o /tmp/passportos-test-manager
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-manager
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Itests/manager_stubs -Icomponents/passport_core/include tests/test_boot_hook.c \
        components/passport_core/passport_model.c -lcrypto -o /tmp/passportos-test-boot-hook
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-boot-hook
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Itests/manager_stubs -Icomponents/passport_core/include -Icomponents/passport_sdk/include \
        tests/test_qso_migration.c components/passport_core/passport_model.c \
        components/passport_core/passport_qso_data.c -o /tmp/passportos-test-qso-migration
    fixture_dir="$(mktemp -d /tmp/passportos-qso-fixture.XXXXXX)"
    python3 tools/qso_settings.py export tests/fixtures/qso-legacy-synthetic.bin \
        --output "$fixture_dir/synthetic.qso-settings"
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-qso-migration "$fixture_dir/synthetic.qso-settings"
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -Itests/manager_stubs -Icomponents/passport_core/include -Icomponents/passport_sdk/include \
        tests/test_sdk.c components/passport_core/passport_model.c -o /tmp/passportos-test-sdk
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-sdk
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -DPASSPORT_TEST_REAL_SHA -Itests/manager_stubs -Imain \
        -Icomponents/passport_core/include -Icomponents/passport_sdk/include \
        tests/test_compat.c components/passport_core/passport_model.c -lcrypto -o /tmp/passportos-test-compat
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-compat
    lifecycle_dir="$(mktemp -d /tmp/passportos-lifecycle.XXXXXX)"
    python3 tests/lifecycle_fixtures.py "$lifecycle_dir"
    node tests/test_community_inputs.cjs build/community-inputs "$lifecycle_dir"
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -DPASSPORT_TEST_REAL_SHA -Itests/manager_stubs -Imain \
        -Icomponents/passport_core/include -Icomponents/passport_sdk/include \
        tests/test_lifecycle.c components/passport_core/passport_model.c \
        components/passport_core/passport_qso_data.c -lcrypto -o /tmp/passportos-test-lifecycle
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-lifecycle "$lifecycle_dir"
    node tests/community_layout_fixtures.cjs "$lifecycle_dir" "${PASSPORT_COMMUNITY_INPUTS:-}"
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -DPASSPORT_TEST_REAL_SHA -Itests/manager_stubs -Imain \
        -Icomponents/passport_core/include -Icomponents/passport_sdk/include \
        tests/test_custom_layout.c components/passport_core/passport_model.c \
        components/passport_core/passport_qso_data.c -lcrypto -o /tmp/passportos-test-custom
    ASAN_OPTIONS=detect_leaks=0 /tmp/passportos-test-custom "$lifecycle_dir"
    python3 -m unittest discover -s tests -p 'test_*.py' -v
    node tests/test_web_ui.cjs
    node tests/test_bin_import.cjs
fi
if [[ "$mode" != --static ]]; then
    idf.py --version
    configuration_dir="$(mktemp -d /tmp/passportos-config.XXXXXX)"
    SDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults" \
        idf.py -B build -D "SDKCONFIG=$configuration_dir/sdkconfig" build
    cp "$configuration_dir/sdkconfig" build/validated.sdkconfig
    idf.py merge-bin -o PassportOS-full.bin
    cp build/PassportOS-full.bin build/PassportOS-fresh.bin
    python3 tools/prepare_fresh.py build/PassportOS-fresh.bin
    python3 tools/verify_firmware.py build
    python3 tools/archive_firmware.py create build --archive-root build/firmware
fi
