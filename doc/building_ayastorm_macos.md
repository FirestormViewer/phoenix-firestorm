# AYAstorm Mac版ビルド手順

Date: 2026-05-06
Target example: `feature/macos-arm64-build-on-latest` (based on `ayastorm-release`)
Output example: `Phoenix-FirestormOS-AYAstorm-release_arm64-7-2-4-80834.dmg`

この文書は AYAstorm の macOS arm64 配布用 DMG を作成するための手順です。Firestorm 本体の一般的な macOS ビルド要件は `doc/building_macos.md` も参照してください。

## 前提

- macOS 15.x
- Xcode 16.x
- CMake
- Python 3.9 互換の venv
- `autobuild`
- `fs-build-variables`
- FMOD Studio API installer for macOS
- FMOD を有効化する場合は、FMOD package を登録した `my_autobuild.xml`

作業ディレクトリ例:

```bash
export WORK="$HOME/work_ayastorm"
export REPO="$WORK/phoenix-firestorm"
export FS_BUILD_VARIABLES="$WORK/fs-build-variables/variables"
export FMOD_REPO="$WORK/3p-fmodstudio"
export TARGET_REF="feature/macos-arm64-build-on-latest"
export AYA_BUILD_ID="80834"
```

`AYA_BUILD_ID` は Release ページの成果物名に含める autobuild build id です。Release 配布物では、git commit count ではなく CI / autobuild 側の build id を明示して揃えます。

## ソース取得

```bash
mkdir -p "$WORK"
cd "$WORK"

git clone https://github.com/mayatonton/phoenix-firestorm.git "$REPO"
git clone https://github.com/FirestormViewer/fs-build-variables.git "$WORK/fs-build-variables"

cd "$REPO"
git checkout "$TARGET_REF"
git status --short
```

既存 worktree を使う場合は `REPO` にその path を指定し、ビルド前に `git status --short` で差分を確認します。不要な差分を含めたまま Release ビルドしないようにしてください。

## Python / autobuild

```bash
cd "$REPO"

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
autobuild --version
```

`xcodebuild` から起動される Python が venv の package を参照できるよう、site-packages を `PYTHONPATH` に渡します。

```bash
export PYTHON_SITE="$(python -c 'import site; print(site.getsitepackages()[0])')"
export PYTHONPATH="$PYTHON_SITE"
```

## FMOD Studio API

AYAstorm の Release ビルドでは `--fmodstudio` を付けて configure します。そのため、事前に FMOD Studio API を autobuild package 化し、`my_autobuild.xml` に登録しておく必要があります。

FMOD Studio API は FMOD 公式サイトから macOS 版を取得します。FMOD Studio Tool ではなく、FMOD Studio API installer を使ってください。

この作業環境では FMOD package 作成に既存の local clone を使います。

```bash
export FMOD_REPO="$WORK/3p-fmodstudio"
test -d "$FMOD_REPO/.git"
git -C "$FMOD_REPO" remote -v
```

ダウンロードした macOS 版 FMOD Studio API installer の `.dmg` を `$FMOD_REPO` に置きます。

```bash
cd "$FMOD_REPO"
ls -lh *.dmg

autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
cat result.txt
```

`result.txt` に package path と md5 hash が出力されます。作成された `fmodstudio-*-darwin64-*.tar.bz2` を viewer 側の `my_autobuild.xml` に登録します。

```bash
cd "$REPO"
cp -n autobuild.xml my_autobuild.xml
export AUTOBUILD_CONFIG_FILE="my_autobuild.xml"

export FMOD_PACKAGE="$(find "$FMOD_REPO" -maxdepth 1 -name 'fmodstudio-*-darwin64-*.tar.bz2' -print -quit)"
export FMOD_HASH="<result.txt の md5 hash>"

autobuild installables edit fmodstudio platform=darwin64 \
  hash="$FMOD_HASH" \
  url="file://$FMOD_PACKAGE"
```

登録後、`my_autobuild.xml` の `fmodstudio` / `darwin64` が作成した package を指していることを確認します。

```bash
rg -n 'fmodstudio|darwin64|file://' my_autobuild.xml
```

FMOD を使わないビルドにする場合は、以降の configure から `--fmodstudio` を外してください。

## 環境変数

```bash
cd "$REPO"

export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
export AUTOBUILD_BUILD_ID="$AYA_BUILD_ID"
export AUTOBUILD_VARIABLES_FILE="$FS_BUILD_VARIABLES"
export AUTOBUILD_CONFIG_FILE="my_autobuild.xml"
export CLANG_MODULE_CACHE_PATH="$REPO/build-darwin-universal/ModuleCache"
```

## Configure

クリーンに作り直す場合:

```bash
cd "$REPO"
rm -rf build-darwin-universal
```

configure:

```bash
autobuild configure -A 64 -c ReleaseFS_open -- \
  --fmodstudio \
  --openal \
  --package \
  --chan AYAstorm-release \
  -DLL_TESTS:BOOL=FALSE
```

configure 後に主要な設定を確認します。

```bash
rg -n 'CMAKE_BUILD_TYPE|ADDRESS_SIZE|CMAKE_OSX_ARCHITECTURES|VIEWER_CHANNEL|USE_FMODSTUDIO|USE_OPENAL|OPENSIM|PACKAGE|VIEWER_BINARY_NAME' \
  build-darwin-universal/CMakeCache.txt
```

期待値の例:

```text
ADDRESS_SIZE:STRING=64
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_OSX_ARCHITECTURES:STRING=arm64;x86_64
OPENSIM:BOOL=ON
PACKAGE:BOOL=ON
USE_FMODSTUDIO:BOOL=ON
USE_OPENAL:BOOL=ON
VIEWER_BINARY_NAME:STRING=ayastorm-bin
VIEWER_CHANNEL:STRING=Firestorm-AYAstorm-release
```

## Build / Package

`llpackage` scheme を Release で実行します。

```bash
cd "$REPO/build-darwin-universal"

DEVELOPER_DIR="$DEVELOPER_DIR" \
AUTOBUILD_BUILD_ID="$AYA_BUILD_ID" \
PYTHONPATH="$PYTHONPATH" \
AUTOBUILD_VARIABLES_FILE="$FS_BUILD_VARIABLES" \
AUTOBUILD_CONFIG_FILE="$AUTOBUILD_CONFIG_FILE" \
CLANG_MODULE_CACHE_PATH="$CLANG_MODULE_CACHE_PATH" \
xcodebuild \
  -project Firestorm.xcodeproj \
  -scheme llpackage \
  -configuration Release \
  -derivedDataPath "$REPO/build-darwin-universal/DerivedData" \
  CLANG_MODULE_CACHE_PATH="$CLANG_MODULE_CACHE_PATH" \
  build
```

DMG 作成時に `hdiutil create` が `装置が構成されていません` で失敗する場合は、サンドボックスや権限の制約でディスクイメージ操作が止まっています。同じコマンドを通常の Terminal から実行してください。

## 成果物

```bash
export DMG="$REPO/build-darwin-universal/newview/Phoenix-FirestormOS-AYAstorm-release_arm64-7-2-4-${AYA_BUILD_ID}.dmg"
export APP="$REPO/build-darwin-universal/newview/Release/AYAstorm.app"

ls -lh "$DMG"
```

成果物の例:

```text
build-darwin-universal/newview/Phoenix-FirestormOS-AYAstorm-release_arm64-7-2-4-80834.dmg
```

## 検証

ローカル app / DMG の基本検証:

```bash
hdiutil verify "$DMG"
codesign --verify --deep --strict --verbose=2 "$APP"
lipo -archs "$APP/Contents/MacOS/AYAstorm"
/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP/Contents/Info.plist"
```

期待値:

```text
hdiutil verify: VALID
codesign: valid on disk / satisfies its Designated Requirement
lipo: arm64
CFBundleIconFile: ayastorm_icon.icns
CFBundleVersion: 7.2.4.80834
CFBundleShortVersionString: 7.2.4.80834
```

ログイン情報やローカル環境情報が混入していないか確認します。いずれも出力なしであることを確認してください。

```bash
export LOCAL_USER="$(id -un)"

rg -a -l "$LOCAL_USER|/Users/$LOCAL_USER" "$APP/Contents"

find "$APP" \
  -iname 'account_history*' \
  -o -iname 'bin_conf.dat' \
  -o -iname 'cookies*' \
  -o -iname '*credentials*' \
  -o -iname '*saved_password*'
```

DMG 内の app も確認します。

```bash
hdiutil attach -nobrowse -readonly "$DMG"
```

`hdiutil attach` の出力から device と mount point を確認し、以下の `diskX` を置き換えます。

```bash
export DMG_MOUNT="/Volumes/AYAstorm Installer"
export DMG_APP="$DMG_MOUNT/FirestormOS-AYAstorm-release.app"

codesign --verify --deep --strict --verbose=2 "$DMG_APP"
rg -a -l "$LOCAL_USER|/Users/$LOCAL_USER" "$DMG_APP/Contents"
find "$DMG_APP" \
  -iname 'account_history*' \
  -o -iname 'bin_conf.dat' \
  -o -iname 'cookies*' \
  -o -iname '*credentials*' \
  -o -iname '*saved_password*'

md5 "$DMG_MOUNT/.VolumeIcon.icns" \
  "$REPO/indra/newview/icons/ayastorm/ayastorm_icon.icns"

hdiutil detach -force /dev/diskX
```

## 署名とローカルパス対策

`Code Signature Invalid` やローカルビルドパス混入を避けるため、Release package では Mach-O のローカルシンボルを落としてから再署名します。

- `indra/cmake/00-Common.cmake`
  - Darwin / Clang で `-ffile-prefix-map`, `-fmacro-prefix-map`, `-fdebug-prefix-map` を指定
- `indra/newview/viewer_manifest.py`
  - package 時に bundle 内 Mach-O へ `strip -S -x` を実行
  - その後に nested app / dylib を再署名

`strip` の途中で code signature が無効になる警告が出ることがありますが、その直後に再署名するため想定内です。配布前には必ず `codesign --verify --deep --strict` を通してください。

## 注意

- DMG ファイル名は Release 向けに `arm64` を含めます。
- アプリ名は `AYAstorm.app` のままです。
- DMG 内の app 名は `FirestormOS-AYAstorm-release.app` です。
- 配布前に notarization を行う場合は、Developer ID と notary 設定を別途確認します。
- 生成済み app を手作業で編集した場合は、必ず再署名と `codesign --verify --deep --strict` を実行します。
