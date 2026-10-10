#!/bin/sh
# Build a skeleton .deb. Python dependencies are not vendored; see packaging/README.md.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VERSION=$(sed -n 's/^version = "\(.*\)"/\1/p' "$ROOT/pyproject.toml" | head -n 1)
STAGE=$(mktemp -d)
OUT="$ROOT/dist"
mkdir -p "$OUT" \
    "$STAGE/DEBIAN" \
    "$STAGE/usr/bin" \
    "$STAGE/usr/lib/voxprint-lucent" \
    "$STAGE/usr/share/applications"
cp -a "$ROOT/voxprint_lucent" "$STAGE/usr/lib/voxprint-lucent/"
find "$STAGE" -type d -name __pycache__ -exec rm -rf {} +
cat > "$STAGE/usr/bin/voxprint-lucent" << 'EOF'
#!/bin/sh
export PYTHONPATH="/usr/lib/voxprint-lucent${PYTHONPATH:+:$PYTHONPATH}"
exec python3 -m voxprint_lucent "$@"
EOF
chmod 755 "$STAGE/usr/bin/voxprint-lucent"
cp "$ROOT/packaging/linux/ai.voxprint.Lucent.desktop" \
    "$STAGE/usr/share/applications/ai.voxprint.Lucent.desktop"
cat > "$STAGE/DEBIAN/control" << EOF
Package: voxprint-lucent
Version: ${VERSION}
Section: graphics
Priority: optional
Architecture: amd64
Depends: python3 (>= 3.14)
Maintainer: Aleksandr Mitroshenkov <223849179+Mitroshenkov87@users.noreply.github.com>
Description: Voxprint AI Lucent image viewer
 Fast, colour-accurate image viewer for the Voxprint AI Media Suite.
 Does not require an NVIDIA GPU. This skeleton expects Python 3.14 and
 the project dependencies. The release package will ship a Nuitka onedir build.
EOF
dpkg-deb --root-owner-group --build "$STAGE" "$OUT/voxprint-lucent_${VERSION}_amd64.deb"
rm -rf "$STAGE"
echo "Wrote $OUT/voxprint-lucent_${VERSION}_amd64.deb"
