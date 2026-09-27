#!/usr/bin/env bash
# .bi language installer
#   curl -fsSL https://github.com/YOURNAME/bi/raw/main/install.sh | bash
set -euo pipefail

REPO="BarshanSarkar/bi"                 # <-- CHANGE THIS to your GitHub user/repo
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

BIN="bi-${OS}-${ARCH}"

# ---- resolve URL ----
if [ "$VERSION" = "latest" ]; then
  URL="https://github.com/${REPO}/releases/latest/download/${BIN}"
else
  URL="https://github.com/${REPO}/releases/download/${VERSION}/${BIN}"
fi

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
