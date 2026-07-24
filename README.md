# PWMK Firmware (Pico W Mechanical Keyboard Firmware)

- PWMK Firmwareは、Raspberry Pi Pico W上で動作するキーボードファームウェアです。
- キーボード、マウス、リモートコントローラーなどの機能を持つ多機能入力デバイス用ファームウェアです。
- カスタマイズ可能なキー配置と多様な入力方法を提供します。

## 特徴

### 他のファームウェアより優れている点

- Raspberry Pi Pico W によるオンボードチップのBLE接続と、TinyUSBを利用したUSB接続の両方に対応しています。
- これはQMKやZMK、KMKなどの他の主要なキーボードファームウェアではサポートされていません。(2026年7月現在)
- Pico-SDK以外の外部ライブラリに依存していません。
- ほぼすべてのコードのライセンスはMITライセンスで、商用利用も可能です。

### 他のファームウェアに劣っている点

- 上記以外ほぼ全てです。
- 他のファームウェアライブラリではライセンスが厳しい、商用利用できない、外部ライブラリに依存している、BLE通信にプロプライエタリなコードが必要であるなどの制約がありますが、
  PWMK Firmwareはそれらの制約を回避することを目的としています。
  逆に、Picoで有線接続を使用する場合や、他のマイコンを使用する場合は、上記のようなファームウェアを使用するべきです。

## ビルド手順

- VS Code拡張を使用する方法と、CLIでビルドする方法の2種類があります。
- どちらの方法でも、ビルド前に `uv run tools/pwmk.py profile <profile>` でプロファイルを選択します。
- 手順一覧については [ビルド手順.md](docs/ビルド手順.md) を参照してください。
- VS Code版は [ビルド手順_VSCode.md](docs/ビルド手順_VSCode.md)、CLI版は [ビルド手順_CLI.md](docs/ビルド手順_CLI.md) に分かれています。
- プロファイルについての詳細は [内部処理_プロファイルシステム.md](docs/内部処理_プロファイルシステム.md) を参照してください。

## ビルドテスト

- CLI ビルドの検証用スクリプトとして [tools/test_build.py](tools/test_build.py) を用意しています。
- 手順は [ビルドテスト手順.md](docs/ビルドテスト手順.md) を参照してください。

## 使い方

- まだ開発中のため、ドキュメントは不完全です。詳しい使用方法はソースコードを直接参照してください。
- 設定は `users/<profile>/profile.yaml` と `users/<profile>/users.c` で管理します。
- ボード設定、キーマップ、USB/BLE の有効化、マウスキーやトラックパッドの設定は `users/<profile>/profile.yaml` で行います。
- ユーザ定義のイベント処理は `users/<profile>/users.c` で行います。
- 使用できるキーコードは [src/keyboard/code.h](src/keyboard/code.h) に定義されています。
- 現行の標準プロファイルは `users/remopicon_v1` です。
- `remopicon_v1`では、キーマトリクス、トラックパッド Cirque Pinnacle、フルカラー LED WS2812B を使用する前提です。
