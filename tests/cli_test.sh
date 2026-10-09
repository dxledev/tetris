#!/usr/bin/env bash
set -euo pipefail

binary="${1:?Pass the tetris executable}"
test_directory="$(/usr/bin/mktemp -d /tmp/tetris-cli-test.XXXXXX)"
config_directory="$test_directory/config"
export QT_QPA_PLATFORM=offscreen
unset DISPLAY WAYLAND_DISPLAY

fail() {
    printf 'cli-test: %s\n' "$*" >&2
    exit 1
}

theme() {
    "$binary" --config-dir "$config_directory" theme "$@"
}

theme show > "$test_directory/default.json"
[[ ! -e "$config_directory" ]] || fail 'show created configuration files'
theme preset paper --dry-run > "$test_directory/dry-run.json"
[[ ! -e "$config_directory" ]] || fail 'dry run created configuration files'
/usr/bin/jq -e '.name == "paper"' "$test_directory/dry-run.json" >/dev/null
theme preset slate >/dev/null
theme set 'accent=#ff2233' 'piece_t=#8833ff' >/dev/null
/usr/bin/jq -e '.colors.accent == "#ff2233" and .colors.piece_t == "#8833ff" and (.source | not)' "$config_directory/theme.json" >/dev/null
before="$(/usr/bin/sha256sum "$config_directory/theme.json")"
if theme set 'accent=#bad' >/dev/null 2>&1; then fail 'invalid color succeeded'; fi
[[ "$before" == "$(/usr/bin/sha256sum "$config_directory/theme.json")" ]] || fail 'invalid color changed theme'
theme export "$test_directory/export.json"
theme preset forest >/dev/null
theme import "$test_directory/export.json" >/dev/null
/usr/bin/cmp "$config_directory/theme.json" "$test_directory/export.json"
theme follow file "$test_directory/export.json" >/dev/null
/usr/bin/jq -e '.source.kind == "file"' "$config_directory/theme.json" >/dev/null
theme detach >/dev/null
/usr/bin/jq -e 'has("source") | not' "$config_directory/theme.json" >/dev/null
if theme follow file "$config_directory/theme.json" >/dev/null 2>&1; then fail 'self-follow succeeded'; fi
if "$binary" --mode unsupported >/dev/null 2>&1; then fail 'invalid mode succeeded'; fi
if "$binary" --seed -1 >/dev/null 2>&1; then fail 'invalid seed succeeded'; fi
if "$binary" --dry-run >/dev/null 2>&1; then fail 'game accepted dry-run'; fi
theme set 'accent=#112233' >/dev/null &
first_pid=$!
theme set 'piece_o=#445566' >/dev/null &
second_pid=$!
wait "$first_pid"
wait "$second_pid"
/usr/bin/jq -e '.colors.accent == "#112233" and .colors.piece_o == "#445566"' "$config_directory/theme.json" >/dev/null
printf 'CLI checks passed.\n'
