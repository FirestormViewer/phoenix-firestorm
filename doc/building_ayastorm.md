# Ayastorm Viewer ビルド手順書

Linux版 / Windows版
2026年4月

---

## Linux版ビルド手順

### 1. 必要な環境

- Ubuntu 22.04 LTS (x86_64)
- RAM 16GB以上、ストレージ 64GB以上
- GCC 11（Ubuntu 22.04のデフォルト）
- Python 3（venv使用推奨）

### 2. 必要パッケージのインストール（一度だけ）

```bash
sudo apt install libgl1-mesa-dev libglu1-mesa-dev libpulse-dev build-essential \
  python3-pip git libssl-dev libxinerama-dev libxrandr-dev \
  libfontconfig-dev libfreetype6-dev gcc-11 cmake
```

### 3. ディレクトリ作成とリポジトリのclone

```bash
mkdir ~/work_ayastorm && cd ~/work_ayastorm
git clone https://github.com/mayatonton/phoenix-firestorm.git
cd phoenix-firestorm
git checkout ayastorm-release

# ビルド変数リポジトリ
cd ~/work_ayastorm
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

### 4. Python仮想環境とautobuildのセットアップ（一度だけ）

```bash
cd ~/work_ayastorm/phoenix-firestorm
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### 5. 環境変数の設定

毎回ビルド前に実行するか `~/.bashrc` に追記しておく：

```bash
source ~/work_ayastorm/phoenix-firestorm/.venv/bin/activate
export AUTOBUILD_VARIABLES_FILE=$HOME/work_ayastorm/fs-build-variables/variables
```

### 6. FMODのセットアップ（一度だけ）

https://www.fmod.com で無料アカウントを作成してLinux版 **FMOD Studio API** をダウンロード（バージョン2.03.07）。

```bash
cd ~/work_ayastorm
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
cp ~/ダウンロード/fmodstudioapi20307linux.tar.gz ~/work_ayastorm/3p-fmodstudio/
cd ~/work_ayastorm/3p-fmodstudio
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
cat result.txt  # md5値を確認
```

result.txt の md5 値を確認してFirestormに登録：

```bash
cd ~/work_ayastorm/phoenix-firestorm
autobuild installables edit fmodstudio platform=linux64 \
  hash=<md5値> \
  url=file:///home/{user name}/work_ayastorm/3p-fmodstudio/fmodstudio-2.03.07-linux64-*.tar.bz2
```

### 7. configure（初回または --clean のとき）

```bash
cd ~/work_ayastorm/phoenix-firestorm
autobuild configure -A 64 -c ReleaseFS_open --   --fmodstudio -DLL_TESTS:BOOL=FALSE --package --chan AYAstorm-release
```

### 8. ビルド

```bash
autobuild build -A 64 -c ReleaseFS_open --no-configure
```

### 9. キャッシュの削除 & インストール
autobuild configure -A 64 -c ReleaseFS_open --   --fmodstudio -DLL_TESTS:BOOL=FALSE --package --chan AYAstorm-release
autobuild build -A 64 -c ReleaseFS_open

cd ~/work_ayastorm/phoenix-firestorm/build-linux-x86_64/newview/packaged
rm -rf ~/ayastorm/
rm -rf ~/.local/share/applications/ayastorm-viewer.desktop
./install.sh
rm -rf ~/.ayastorm_x64/cache/

### 10. 実行
```bash
~/ayastorm/ayastorm 
```




---

## Windows版ビルド手順

### 1. 必要ツールのインストール（一度だけ）

> **重要：** すべての作業はPowerShellではなく **cmd.exe（コマンドプロンプト）管理者モード** で行う。

#### Visual Studio 2022 Community（無料）

- 管理者として実行
- 「Desktop development with C++」にチェック

#### Git for Windows

- 「Checkout as-is, commit as-is」を選択（**重要！**）

#### CMake 4.1.2以上

- 「Add CMake to the system PATH for all users」を選択

#### Cygwin 64

- 管理者として実行
- 追加パッケージ：`Devel/patch` を選択

#### Python 3

- 管理者として実行
- 「Add Python to PATH」にチェック
- インストール先：`C:\Python3`

#### NSIS（インストーラー作成用）

- https://nsis.sourceforge.io からダウンロード

### 2. リポジトリのclone

```cmd
c:
mkdir work_ayastorm
cd work_ayastorm
git clone https://github.com/mayatonton/phoenix-firestorm.git
cd phoenix-firestorm
git checkout ayastorm-release

cd c:\work_ayastorm
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

### 3. autobuildのセットアップ（一度だけ）

```cmd
cd c:\work_ayastorm\phoenix-firestorm
pip install -r requirements.txt
```

### 4. 環境変数の設定

ビルドのたびに **管理者cmd** で以下を実行：

```cmd
set PYTHONUTF8=1
set AUTOBUILD_VSVER=170
set AUTOBUILD_VARIABLES_FILE=c:\work_ayastorm\fs-build-variables\variables
set PATH=C:\cygwin64\bin;%PATH%
set AUTOBUILD_CONFIG_FILE=my_autobuild.xml
```

> `my_autobuild.xml` はFMODセットアップ後に作成されます。

### 5. FMODのセットアップ（一度だけ）

https://www.fmod.com で無料アカウントを作成してWindows版 **FMOD Studio API** をダウンロード（バージョン2.03.07）。

```cmd
cd c:\work_ayastorm
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
copy fmodstudioapi20307win-installer.exe c:\work_ayastorm\3p-fmodstudio\
cd c:\work_ayastorm\3p-fmodstudio
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
type result.txt
```

result.txt の md5 値を確認してFirestormに登録：

```cmd
cd c:\work_ayastorm\phoenix-firestorm
copy autobuild.xml my_autobuild.xml
set AUTOBUILD_CONFIG_FILE=my_autobuild.xml
autobuild installables edit fmodstudio platform=windows64 ^
  hash=<md5値> ^
  url=file:///c:/work_ayastorm/3p-fmodstudio/fmodstudio-2.03.07-windows64-*.tar.bz2
```

### 6. configure (Legacy)

管理者cmdで環境変数を設定した後に実行：

```cmd
cd c:\work_ayastorm\phoenix-firestorm
rmdir /s /q build-vc170-64
autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio -DLL_TESTS:BOOL=FALSE --package --chan AYAstorm-release
```

### 7. ビルド (Legacy)

```cmd
autobuild build -A 64 -c ReleaseFS_open --no-configure
```

### 8. インストーラーの場所 (Legacy)

```
c:\work_ayastorm\phoenix-firestorm\build-vc170-64\newview\Release\
Phoenix-FirestormOS-Ayastorm-release_LEGACY-7-2-4-80621_Setup.exe
```

### 9. configure (AVX2)

管理者cmdで環境変数を設定した後に実行：

```cmd
cd c:\work_ayastorm\phoenix-firestorm
rmdir /s /q build-vc170-64
autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio --avx2 -DLL_TESTS:BOOL=FALSE --package --chan AYAstorm-release
```

### 10. ビルド (AVX2)

```cmd
autobuild build -A 64 -c ReleaseFS_AVX2 --no-configure
```

### 11. インストーラーの場所 (AVX2)

```
c:\work_ayastorm\phoenix-firestorm\build-vc170-64\newview\Release\
Phoenix-FirestormOS-AYAstorm-release_AVX2-7-2-4-80621_Setup.exe
```

