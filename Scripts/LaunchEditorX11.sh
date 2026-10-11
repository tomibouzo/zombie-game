#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor_binary="${UE_EDITOR:-${UE5_ROOT:-$HOME/Documents/UE5}/Engine/Binaries/Linux/UnrealEditor}"

if [[ ! -x "$editor_binary" ]]; then
    printf 'Unreal Editor not found at %s\nSet UE_EDITOR to its executable path or UE5_ROOT to the engine directory.\n' "$editor_binary" >&2
    exit 1
fi

export SDL_VIDEODRIVER=x11
exec "$editor_binary" "$project_root/prototype3.uproject" "$@"
