# CLI版ビルド手順

## はじめに

このガイドでは、pwmk_firmware をCLIでビルドする手順を説明します。

## 1. 前提環境

このスクリプトは Ubuntu で動作します。
テスト済みの環境は以下のとおりです。

- Ubuntu 24.04 LTS

`python3` は事前に必要です。その他のビルド依存は、ビルドスクリプトが自動導入します。

ビルド依存パッケージは以下のとおりです。

- `cmake`
- `ninja`
- `gcc-arm-none-eabi`
- `git`
- `libnewlib-arm-none-eabi`
- `libstdc++-arm-none-eabi-newlib`
- `python3`

## 2. ビルドの実行

リポジトリルートで次のコマンドを実行します。

```bash
python3 tools/pwmk.py build --clean
```

root でない通常ユーザーで依存導入も自動化する場合は、`sudo` が使える必要があります。すでに依存が入っている環境では、そのままビルドだけ実行されます。

`pico-sdk` は既定で git から自動取得され、`$HOME/.pwmk/pico-sdk-<sdk-tag>` 配下に配置されます。
BLE と USB は既存の CMake オプションをそのまま利用できます。

- USBとBLEの両方を有効にする: 既定値のまま実行します。
- USBのみ有効にする: `python3 tools/pwmk.py build --enable-ble OFF`
- BLEのみ有効にする: `python3 tools/pwmk.py build --enable-usb OFF`

既存の SDK を使いたい場合は `--sdk-path /path/to/pico-sdk` を指定します。

自動依存導入を行わず、既存環境だけでビルドしたい場合は `--skip-deps` を指定します。

成果物は [build/cli](../build/cli) に出力されます。

## 3. 実行例

```bash
apt-get update
apt-get install -y --no-install-recommends python3
python3 tools/pwmk.py build --clean
```
