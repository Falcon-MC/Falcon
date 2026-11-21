#!/usr/bin/env bash
# installs the pinned windows toolchain from toolchain.txt, keeping the downloaded packages in the given folder
set -euo pipefail

list="$(dirname "$0")/toolchain.txt"
packages="${1:?usage: install.sh <package folder>}"
mirror=https://repo.msys2.org/mingw/mingw64
mkdir -p "$packages"

files=()
while read -r file; do
    # a windows checkout ends every line with a carriage return
    file="${file%$'\r'}"
    [[ -z "$file" || "$file" == \#* ]] && continue
    # the signature comes along so pacman still checks every package
    for part in "$file" "$file.sig"; do
        [[ -f "$packages/$part" ]] || curl -fsSL --retry 3 -o "$packages/$part" "$mirror/$part"
    done
    files+=("$packages/$file")
done < "$list"

pacman -U --noconfirm --needed "${files[@]}"
# git only reads the build number: it does not compile anything, so it is not pinned
pacman -S --noconfirm --needed git
