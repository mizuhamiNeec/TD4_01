# 起動方法

- Editor:
  - `UnnamedEditorApp.exe --game=<GameName>`

!!! note "Note"
    `<GameName>` は premakeなどで定義したゲーム名 (例: `ParkourGame`) を指定してください。

- Game:
    - premake で定義したゲームプロジェクトの exe を直接起動

補助オプション:

- `--repo-root=<path>`: manifest 探索の repo root を明示
- `--validate-startup-only`: 起動導線検証だけ行って終了