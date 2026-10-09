#!/usr/bin/env bash
set -euo pipefail

project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd)"
build_directory="$project_directory/build"
output_directory="$build_directory/packages"
dry_run=0

usage() {
    printf '%s\n' 'Usage: package.sh [--dry-run] [--build-dir DIR] [--output-dir DIR]'
}

fail() {
    printf 'package: %s\n' "$*" >&2
    exit 1
}

parse_options() {
    while (( $# )); do
        case "$1" in
            --help|-h) usage; exit 0 ;;
            --dry-run) dry_run=1; shift ;;
            --build-dir|--output-dir)
                (( $# >= 2 )) || fail "Missing value for $1"
                [[ "$2" == /* && "$2" != *$'\n'* ]] || fail "Expected an absolute path for $1"
                if [[ "$1" == --build-dir ]]; then build_directory="${2%/}"; else output_directory="${2%/}"; fi
                shift 2 ;;
            *) fail "Unknown option: $1" ;;
        esac
    done
}

create_package() {
    local version architecture name temporary stage archive
    [[ -x "$build_directory/tetris" ]] || fail "Build the app first: $build_directory/tetris"
    version="$("$build_directory/tetris" --version)"
    version="${version#Tetris }"
    [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail 'Unexpected application version'
    architecture="$(/usr/bin/uname -m)"
    name="tetris-$version-linux-$architecture"
    archive="$output_directory/$name.tar.gz"
    if (( dry_run )); then
        printf 'Package executable: %s/tetris\nArchive: %s\n' "$build_directory" "$archive"
        return
    fi
    /usr/bin/mkdir -p -- "$output_directory"
    temporary="$(/usr/bin/mktemp -d "$output_directory/.package.XXXXXX")"
    stage="$temporary/$name"
    /usr/bin/mkdir -p -- "$stage/scripts"
    /usr/bin/install -m755 -- "$build_directory/tetris" "$stage/tetris"
    /usr/bin/install -m755 -- "$project_directory/scripts/install-desktop.sh" "$stage/scripts/install-desktop.sh"
    /usr/bin/install -m644 -- "$project_directory/README.md" "$project_directory/LICENSE" "$stage/"
    /usr/bin/cp -R -- "$project_directory/assets" "$project_directory/themes" "$stage/"
    /usr/bin/tar -czf "$temporary/archive.tar.gz" -C "$temporary" "$name"
    /usr/bin/mv -T -- "$temporary/archive.tar.gz" "$archive"
    printf '%s\n' "$archive"
}

parse_options "$@"
create_package
