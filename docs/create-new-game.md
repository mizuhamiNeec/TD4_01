# 新規ゲーム作成ガイド

この手順は「空ゲーム追加 -> 最小シーン作成 -> 独自コンポーネント追加 -> Editor 起動確認」までの最短導線です。

## 1. 雛形生成

`tools/new-game.ps1` を使います。

```powershell
.\tools\new-game.ps1 -Name MyGame -Alias My
```

生成される主な内容:
- `projects/MyGame/runtime` (最小 GameModule + ComponentRegistration + SampleComponent)
- `projects/MyGame/content/scenes/bootstrap.json`
- `projects/MyGame/config/game_profile.json`

## 2. エンジン配線

生成後、以下は手動追加が必要です。

1. `premake5.lua`
   - `MyGameRuntime` を追加
   - `UnnamedEditorApp` に include/link 追加
2. `src/app/EditorMain.cpp`
   - `RegisterGameModule("MyGame", &CreateMyGameGameModule)` を追加
   - 必要なら alias 追加
3. （必要なら）専用 standalone app を追加
   - `TeamGameMain.cpp` 相当のエントリポイントを用意

## 3. 再生成とビルド

```powershell
.\generateallprojects.ps1
msbuild Unnamed.slnx /m /p:Configuration=Debug /p:Platform=x64
```

## 4. 起動確認

```powershell
.\bin\Debug-windows-x86_64\UnnamedEditorApp\UnnamedEditorApp.exe --game=MyGame
```

または導線検証のみ:

```powershell
.\bin\Debug-windows-x86_64\UnnamedEditorApp\UnnamedEditorApp.exe --game=MyGame --validate-startup-only
```

## 5. よくある失敗

- `Unknown component type` が出る
  - `GetStableName()` と scene JSON の `components[].type` が不一致
  - `RegisterGameComponents` で登録漏れ
  - runtime link/premake 追加漏れ

- game が見つからない
  - `projects/<Game>/config/game_profile.json` 不備
  - `gameName` / `aliases` と `--game` の不一致
  - `--repo-root` が誤っている

