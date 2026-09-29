#!/usr/bin/env bash
# .bi language installer
#   curl -fsSL https://github.com/BarshanSarkar/bi/raw/main/install.sh | bash
set -euo pipefail

REPO="BarshanSarkar/bi"
VERSION="${BI_VERSION:-latest}"
INSTALL_DIR="${BI_INSTALL_DIR:-/usr/local/bin}"

# ---- detect platform ----
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "$OS" in
  linux*)  OS=linux ;;
  darwin*) OS=macos ;;
  *) echo "error: unsupported OS: $OS" >&2; exit 1 ;;
esac

ARCH="$(uname -m)"
case "$ARCH" in
  x86_64|amd64)  ARCH=x86_64 ;;
  aarch64|arm64) ARCH=arm64 ;;
  *) echo "error: unsupported arch: $ARCH" >&2; exit 1 ;;
esac

# ---- resolve version if "latest" ----
if [ "$VERSION" = "latest" ]; then
  VERSION=$(curl -sSL "https://api.github.com/repos/${REPO}/releases/latest" \
            | grep -oP '"tag_name":\s*"v\K[^"]+' | head -1 || true)
  [ -z "$VERSION" ] && VERSION="0.6.0"
fi

BIN="bi-${VERSION}-${OS}-${ARCH}"
URL="https://github.com/${REPO}/releases/download/v${VERSION}/${BIN}"

echo "bi installer"
echo "  platform : ${OS}-${ARCH}"
echo "  version  : ${VERSION}"
echo "  url      : ${URL}"
echo

# ---- download ----
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

if command -v curl >/dev/null 2>&1; then
  curl -fL --progress-bar "$URL" -o "$TMP"
elif command -v wget >/dev/null 2>&1; then
  wget -q --show-progress "$URL" -O "$TMP"
else
  echo "error: need curl or wget" >&2; exit 1
fi

chmod +x "$TMP"

# ---- install ----
if [ -w "$INSTALL_DIR" ]; then
  mv "$TMP" "$INSTALL_DIR/bi"
elif command -v sudo >/dev/null 2>&1; then
  echo "installing to $INSTALL_DIR (needs sudo)"
  sudo mv "$TMP" "$INSTALL_DIR/bi"
else
  echo "error: $INSTALL_DIR not writable and sudo not found" >&2
  echo "hint: BI_INSTALL_DIR=\$HOME/.local/bin bash install.sh" >&2
  exit 1
fi

echo
echo "✓ installed: $INSTALL_DIR/bi"
"$INSTALL_DIR/bi" version || true
echo
echo "Try:  bi new myapp && cd myapp && bi serve src/main.bi"