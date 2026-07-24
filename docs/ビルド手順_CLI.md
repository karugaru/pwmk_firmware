# CLI版ビルド手順

## はじめに

このガイドでは、pwmk_firmware をCLIでビルドする手順を説明します。

## 1. 前提環境

このスクリプトは Linux で動作します。
テスト済みの環境は[ビルド手順](ビルド手順.md#cli)に記載されています。

`uv` は事前に必要です。Python 本体と追加ライブラリは `uv` が管理し、その他のビルド依存はビルドスクリプトが利用可能なパッケージマネージャーを自動判定して導入します。

ビルド依存パッケージは以下のとおりです。

- `cmake`
- `ninja`
- `gcc-arm-none-eabi`
- `git`
- `libnewlib-arm-none-eabi`
- `libstdc++-arm-none-eabi-newlib`
- `uv`

## 2. ビルドの実行

リポジトリルートで次のコマンドを実行します。

```bash
uv sync
uv run tools/pwmk.py profile remopicon_v1
uv run tools/pwmk.py build
```

root でない通常ユーザーで依存導入も自動化する場合は、`sudo` が使える必要があります。すでに依存が入っている環境では、そのままビルドだけ実行されます。

`pico-sdk` は既定で git から自動取得され、`$HOME/.pwmk/pico-sdk-<sdk-tag>` 配下に配置されます。
`picotool` も既定で自動取得され、`$HOME/.pwmk/picotool-<tag>` 配下に配置されます。

ビルド設定はプロファイルで管理します。ボード種別や USB/BLE の有効化を変更したい場合は、`users/<profile>/profile.yaml` を編集してください。

未選択状態で `uv run tools/pwmk.py build` を実行するとエラーになります。

既存の SDK を使いたい場合は `--sdk-path /path/to/pico-sdk` を指定します。

自動依存導入を行わず、既存環境だけでビルドしたい場合は `--skip-deps` を指定します。

プロファイル選択とビルドを 1 回で行う場合は、以下を実行します。

```bash
uv run tools/pwmk.py build --profile remopicon_v1
```

成果物は [build/cli](../build/cli) に出力されます。

## 3. 実行例

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync
uv run tools/pwmk.py profile remopicon_v1
uv run tools/pwmk.py build
```
