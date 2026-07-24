# VS Code版ビルド手順

## はじめに

このガイドでは、Visual Studio Codeと Raspberry Pi Pico 拡張を使って pwmk_firmware をビルドし、Raspberry Pi Pico に書き込む手順を説明します。

## 1. リポジトリのクローン

[pwmk_firmware](https://github.com/karugaru/pwmk_firmware)にアクセスし、リポジトリをクローンします。

![buildguide_1](images/buildguide_1.png)

最も簡単な方法は、GitHubのウェブサイトで「Code」ボタンをクリックし、「Download ZIP」を選択してダウンロードすることです。

![buildguide_2](images/buildguide_2.png)

## 2. ビルド環境のセットアップ

Visual Studio Codeをインストールし、必要な拡張機能を追加します。

![buildguide_3](images/buildguide_3.png)

拡張機能マーケットプレイスで、`Raspberry Pi Pico`と検索し、拡張機能をインストールします。
インストールして、クローンしたディレクトリをVisual Studio Codeで開くと自動でビルド環境がセットアップされます。

![buildguide_4](images/buildguide_4.png)

また、[UVのインストール手順](https://docs.astral.sh/uv/getting-started/installation/)に従って、UVをインストールしてください。

## 3. ビルドの実行

ビルド環境がセットアップされたら、まず VS Code のターミナルで使用するプロファイルを選択します。

```powershell
uv run tools/pwmk.py profile remopicon_v1
```

プロファイルを選択したら、Visual Studio Code の Raspberry Pi Pico Project 機能で `Clean CMake` を実行します。
`Clean CMake` が成功したら、`Compile Project` を実行します。

ボード種別や USB/BLE の有効化は CMake オプションではなくプロファイルで管理します。変更したい場合は `users/<profile>/profile.yaml` を編集してください。

![buildguide_5](images/buildguide_5.png)

ビルドが成功すると、下部のターミナルに`Compilation successful`と表示されます。

![buildguide_6](images/buildguide_6.png)

## 4. ファームウェアの書き込み

書き込みを行うにはRaspberry Pi Picoをブートローダーモードで接続する必要があります。
Raspberry Pi PicoをBOOTボタンを押しながらUSBで接続することで、ブートローダーモードで接続されます。

## 4.1. ツールで書き込み

ブートローダーモードで接続した状態で、Visual Studio CodeのRaspberry Pi Pico Project機能で、`Run Project (USB)`を実行します。
実行すると、ビルドしたファームウェアがPicoに書き込まれます。

![buildguide_8](images/buildguide_8.png)

## 4.2. 手動で書き込み

ビルドしたファームウェアは、クローンしたディレクトリの`build`フォルダ内にあります。
拡張子が`.uf2`のファイルをPicoのドライブにドラッグアンドドロップして書き込みます。

![buildguide_7](images/buildguide_7.png)
