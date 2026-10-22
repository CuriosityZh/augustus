#!/usr/bin/env bash
set -euo pipefail
build_dir=${1:-build-planning}
destination=${2:-dist}
mkdir -p "$destination"
cmake --install "$build_dir"
app="$destination/Augustus Planning.app"
# Replacing an output bundle is safe; installed applications are never touched.
if [[ -e "$app" ]]; then
    echo "Output already exists: $app. Choose a new destination." >&2
    exit 1
fi
ditto "$build_dir/augustus.app" "$app"
codesign --force --deep --options runtime --timestamp=none --sign - "$app"
codesign --verify --deep --strict "$app"
file "$app/Contents/MacOS/augustus" | tee "$destination/architecture.txt"
lipo -verify_arch arm64 "$app/Contents/MacOS/augustus"
# Reject accidental references to a developer's Homebrew or original app.
if otool -L "$app/Contents/MacOS/augustus" | grep -E '/opt/homebrew|/usr/local|/Applications/augustus'; then
    echo 'Bundle still refers to external build dependencies' >&2
    exit 1
fi
ditto -c -k --sequesterRsrc --keepParent "$app" "$destination/Augustus-Planning-macOS-arm64.zip"
shasum -a 256 "$destination/Augustus-Planning-macOS-arm64.zip" > "$destination/SHA256SUMS"
