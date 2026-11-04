#!/usr/bin/env bash
# Builds the SteelMC terrain helper and puts everything the game needs into ./steel/ :
#   steel/steel-provider   (the helper program, built from ./steel-provider, AGPL-3.0)
#   steel/block.json       (block-state table of the matching Minecraft data version)
# Needs: curl, git, a C compiler, unzip, internet.  Installs Rust (rustup, user-local) if it is missing.  Safe to re-run; use --force to rebuild.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Which Minecraft version the terrain is: decided by the SteelMC revision pinned in steel-provider/Cargo.toml ("+mc26.3" in Cargo.lock = Minecraft 26.3).
# To use another version pass a SteelMC revision that targets it plus the matching net.minestom:data version, e.g.
#     ./build-steel.sh --rev <SteelMC commit sha> --data 1.21.11-rv1 --force
FORCE=0; LIBMODE=0; REV=""; DATA_VERSION="${HV_DATA_VERSION:-26.3-rv1}"      # default = what the pinned SteelMC targets (upstream mise.toml: minecraft_data_from)
while [[ $# -gt 0 ]]; do case "$1" in
  --force) FORCE=1 ;; --lib) LIBMODE=1 ;; --rev) REV="$2"; shift ;; --data) DATA_VERSION="$2"; shift ;; *) echo "[steel] unknown option $1"; exit 1 ;; esac; shift; done
if [[ -n "$REV" ]]; then
  [[ "$DATA_VERSION" != "26.3-rv1" ]] || { echo "[steel] --rev needs --data <net.minestom:data version of that revision>"; exit 1; }
  sed -i -E "s/(git = \"https:\/\/github.com\/Steel-Foundation\/SteelMC\", rev = \")[0-9a-f]+(\")/\1$REV\2/" "$ROOT/steel-provider/Cargo.toml"
  rm -f "$ROOT/steel-provider/Cargo.lock"; FORCE=1; echo "[steel] pinned SteelMC to $REV"
fi
OUT="$ROOT/steel"; mkdir -p "$OUT"

# ---- 64-bit only: pick the 64-bit Rust target explicitly (never the 32-bit default of a 32-bit userland)
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)            TARGET=x86_64-unknown-linux-gnu ;;
  Linux-aarch64|Linux-arm64) TARGET=aarch64-unknown-linux-gnu ;;
  Darwin-x86_64)           TARGET=x86_64-apple-darwin ;;
  Darwin-arm64)            TARGET=aarch64-apple-darwin ;;
  MINGW64*-x86_64|MSYS*-x86_64|CYGWIN*-x86_64) TARGET=x86_64-pc-windows-gnu ;;   # Windows (MSYS2 / Git Bash); use --target-msvc-style builds by running cargo yourself
  *) echo "[steel] unsupported platform $(uname -s)-$(uname -m): a 64-bit Linux or macOS is required"; exit 1 ;;
esac
[[ "$(getconf LONG_BIT)" == "64" ]] || { echo "[steel] this system is 32-bit - a 64-bit OS is required"; exit 1; }

is64() { command -v file >/dev/null || return 0; file -L "$1" | grep -q "64-bit"; }   # true when the helper binary really is a 64-bit executable
if [[ $FORCE == 0 && $LIBMODE == 1 && -s "$OUT/native-libs.txt" && -s "$OUT/block.json" && ( -f "$OUT/libsteel_provider.a" || -f "$OUT/steel_provider.lib" ) && "$(cat "$OUT/.data-version" 2>/dev/null)" == "$DATA_VERSION" ]]; then echo "[steel] static library already built ($OUT) - use --force to rebuild"; exit 0; fi
if [[ $FORCE == 0 && $LIBMODE == 0 && -x "$OUT/steel-provider" && -s "$OUT/block.json" && "$(cat "$OUT/.data-version" 2>/dev/null)" == "$DATA_VERSION" ]] && is64 "$OUT/steel-provider"; then echo "[steel] already set up ($OUT, 64-bit, data $DATA_VERSION) - use --force to rebuild"; exit 0; fi
[[ -x "$OUT/steel-provider" ]] && ! is64 "$OUT/steel-provider" && echo "[steel] existing helper is not a 64-bit build - rebuilding"
for t in curl git unzip cc; do command -v "$t" >/dev/null || { echo "[steel] missing tool: $t (install it and re-run)"; exit 1; }; done

# ---- Rust (the provider needs the nightly pinned in steel-provider/rust-toolchain.toml; rustup picks it up by itself)
export PATH="$HOME/.cargo/bin:$PATH"
if ! command -v cargo >/dev/null; then
  echo "[steel] installing rustup ..."; curl -fsSL https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain none
  export PATH="$HOME/.cargo/bin:$PATH"
fi

# ---- data jar (block / biome registries)
JAR="$OUT/.data-$DATA_VERSION.jar"
if [[ ! -s "$JAR" ]]; then
  ok=0
  cands=("$DATA_VERSION"); [[ "$DATA_VERSION" == *-rv* ]] && for n in 2 3 4; do cands+=("${DATA_VERSION%-rv*}-rv$n"); done
  for ver in "${cands[@]}"; do for base in "https://repo1.maven.org/maven2" "https://reposilite.bluedragonmc.com/releases"; do
    url="$base/net/minestom/data/$ver/data-$ver.jar"
    echo "[steel] downloading $url"; if curl -fsSL "$url" -o "$JAR.tmp"; then mv "$JAR.tmp" "$JAR"; ok=1; break 2; fi
  done; done
  [[ $ok == 1 ]] || { echo "[steel] could not download net.minestom:data:$DATA_VERSION - put data-$DATA_VERSION.jar at $JAR and re-run"; exit 1; }
fi

# ---- build (from = to: the helper hands out the block-state ids of that one data version, block.json below decodes them)
echo "[steel] building steel-provider (the first build takes several minutes) ..."
( cd "$ROOT/steel-provider" && rustup target add "$TARGET" 2>/dev/null || true; cd "$ROOT/steel-provider" && MINESTOM_DATA_FROM_JAR="$JAR" MINESTOM_DATA_TO_JAR="$JAR" cargo build --release --target "$TARGET" $([[ $LIBMODE == 1 ]] && echo --lib) )
if [[ $LIBMODE == 0 ]]; then
  cp "$ROOT/steel-provider/target/$TARGET/release/steel-provider" "$OUT/steel-provider"
  is64 "$OUT/steel-provider" || { echo "[steel] the built helper is not 64-bit?!"; exit 1; }
fi
unzip -p "$JAR" net/minestom/data/block.json > "$OUT/block.json"
echo "$DATA_VERSION" > "$OUT/.data-version"
MCV="$(grep -A1 'name = "steel-core"' "$ROOT/steel-provider/Cargo.lock" 2>/dev/null | grep -o '+mc[0-9.]*' | head -1)"
if [[ $LIBMODE == 1 ]]; then
  LIBNAME=libsteel_provider.a; [[ -f "$ROOT/steel-provider/target/$TARGET/release/steel_provider.lib" ]] && LIBNAME=steel_provider.lib
  cp "$ROOT/steel-provider/target/$TARGET/release/$LIBNAME" "$OUT/$LIBNAME"
  ( cd "$ROOT/steel-provider" && MINESTOM_DATA_FROM_JAR="$JAR" MINESTOM_DATA_TO_JAR="$JAR" cargo rustc --release --lib --target "$TARGET" -- --print native-static-libs 2>&1 | grep "native-static-libs:" | sed 's/.*native-static-libs: //' | tail -1 > "$OUT/native-libs.txt" ) || true
  [[ -s "$OUT/native-libs.txt" ]] || echo "-lpthread -ldl -lm" > "$OUT/native-libs.txt"
  echo "[steel] static library ready: $OUT/$LIBNAME (native libs: $(cat "$OUT/native-libs.txt"))"
fi
echo "[steel] ready: $OUT  (64-bit, $TARGET, SteelMC ${MCV:-?}, data $DATA_VERSION)"
