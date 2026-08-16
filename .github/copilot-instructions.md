# ルール

## コミットメッセージのルール

コミットメッセージを生成する場合は以下のルールに従ってください。

### コミットメッセージの形式

コミットメッセージは以下の形式で記述してください：
`<タイプ>: <説明>`

### タイプ

以下のいずれかのタイプから適切なものを選択してください：

- feat - 新しい機能を追加する場合
- fix - バグを修正する場合
- refactor - 機能を変更せずにコードを改善する場合（構造化、型追加など）
- docs - ドキュメント、DDL、設定ファイルなどを更新する場合

### 記述ルール

- 説明の詳細さ - 何を変更したのか、なぜ変更したのかを具体的に説明する
- スコープの明記 - 影響を受けるモジュールを説明に含める
- 簡潔さ - 最初の1行で概要が理解できるようにする
- 長さ - あまり長くないように。一行で。

### 良い例

- fix: お気に入りの更新処理で、アップロード日付をベースとして更新するように修正
- feat: メタ情報をページのスクリプトデータから取得するように変更
- refactor: parsePage関数を更新し、ページの解析結果を表す型を追加

## レビューのルール

コードレビューを行う際は以下のルールに従ってください。

## 開発環境・実行境界のルール

- `tools/` の CLI とキーボードテストは Linux で実行する。PowerShell から WSL を呼ぶ場合は、各コマンドを個別に `wsl.exe --` で実行する。
- Windows の VS Code では `.venv_win`、WSL/Linux では既定の `.venv` を使用する。Linux/WSL のビルドは Windows の `PATH` にある SDK や picotool に依存せず、Linux 側の `$HOME/.pwmk` キャッシュを使用する。

### レビュー対象

特に言及がない限り、ステージングされたすべての変更がレビュー対象となります。
レビューはコードの品質、可読性、保守性を向上させることを目的としています。

- 現行の CTest ターゲットは `event_keyboard_test`、`code_convert_test`、`persistence_format_test`、`keymap_persistence_test` の4つです。
- キーボードテストは configure、build、ctest の順に実行し、CTest だけで古いバイナリを実行しないようにします。

### レビューのポイント

修正された内容が全て正しいとは限らないため、注意してレビューを行ってください。

## テストのルール

構成済みのテストを行う場合は、以下のコマンドで実行してください。
cmakeコマンドを直接使用すると失敗する可能性が高いです。

```powershell
wsl.exe -- uv run tools/test_keyboard.py
wsl.exe -- uv run tools/test_profile.py
```

## ビルドのルール

ビルド・ビルドテストを行う場合は、以下の手順に従ってください。
最も優先するのはCLI (WSL2)の手順です。

### CLI (WSL2)

```powershell
wsl.exe -- uv run tools/pwmk.py profile remopicon_v1
wsl.exe -- uv run tools/pwmk.py build
# or
wsl.exe -- uv run tools/pwmk.py profile remopicon_v2_beta
wsl.exe -- uv run tools/pwmk.py build
```

### CLI (Docker on WSL2)

```powershell
wsl.exe -- uv run tools/test_build.py ubuntu_26_04
```

### VS Code (Windows)

`uv run tools/pwmk.py profile remopicon_v1` の後に VS Code タスクの`Clean CMake`、`Compile Project`でビルド

## gitのルール

gitの状態を変更する操作を行わないでください。
`git diff`など、状態を変更しないコマンドでの確認は問題ありません。
