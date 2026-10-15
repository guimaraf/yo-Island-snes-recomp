#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

apply_patch() {
    local repository="$1"
    local patch_file="$2"

    if git -C "$repository" apply --ignore-space-change --check "$patch_file"; then
        git -C "$repository" apply --ignore-space-change "$patch_file"
    elif git -C "$repository" apply --ignore-space-change --reverse --check "$patch_file"; then
        printf 'Patch already applied: %s\n' "$patch_file"
    else
        printf 'Cannot apply patch: %s\n' "$patch_file" >&2
        return 1
    fi
}

apply_patch "$root/modules/recomp-ui" "$root/patches/recomp-ui.patch"
apply_patch "$root/modules/snesrecomp" "$root/patches/snesrecomp.patch"
