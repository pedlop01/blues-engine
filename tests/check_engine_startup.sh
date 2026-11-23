#!/usr/bin/env bash
# Startup only: real supplied resources, dummy SDL drivers, bounded clean exit.
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
data_dir=$(realpath -- "${1:-$repo_dir/../orig-games}")
make -C "$repo_dir" build/tests/check_engine_startup
for game in blues-dos blues-amiga blues2-dos prehistorik2-dos; do
    printf '%s\n' "$game"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20s \
        "$repo_dir/build/tests/check_engine_startup" "$data_dir/$game"
done

# Check both naming conventions without changing or duplicating original data.
staging_dir=$(mktemp -d)
trap 'rm -rf -- "$staging_dir"' EXIT
for resource in "$data_dir/blues2-dos/"*; do
    name=$(basename -- "$resource")
    ln -s -- "$resource" "$staging_dir/${name,,}"
done
printf '%s\n' 'blues2-dos (lowercase aliases)'
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20s \
    "$repo_dir/build/tests/check_engine_startup" "$staging_dir"
