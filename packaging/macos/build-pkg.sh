#!/bin/bash
# Builds QpasaReverb-macOS.pkg from the built plugin bundles.
# Usage: packaging/macos/build-pkg.sh <dir with .vst3 and .component> <version> <output.pkg>
set -euo pipefail

SRC="$1"; VERSION="$2"; OUT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"

make_component () {   # id, bundle, install location, pkg name
    local id="$1" bundle="$2" location="$3" name="$4"
    mkdir -p "$WORK/root-$name"
    cp -R "$SRC/$bundle" "$WORK/root-$name/"
    pkgbuild --analyze --root "$WORK/root-$name" "$WORK/$name.plist"
    # always install to the standard folder, even if an older copy was moved somewhere else
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$name.plist"
    pkgbuild --root "$WORK/root-$name" \
             --component-plist "$WORK/$name.plist" \
             --identifier "$id" \
             --version "$VERSION" \
             --install-location "$location" \
             --scripts "$HERE/scripts" \
             "$WORK/$name.pkg"
}

make_component com.qpasaaudio.qpasareverb.vst3 "Qpasa Reverb.vst3"      "/Library/Audio/Plug-Ins/VST3"       vst3
make_component com.qpasaaudio.qpasareverb.au   "Qpasa Reverb.component" "/Library/Audio/Plug-Ins/Components" au

productbuild --distribution "$HERE/distribution.xml" \
             --resources "$HERE/resources" \
             --package-path "$WORK" \
             --version "$VERSION" \
             "$OUT"

rm -rf "$WORK"
echo "Built $OUT"
