## Summary

- 

## Scope

- [ ] TeamGame runtime/app
- [ ] Engine core/shared
- [ ] Build/CI/docs

## Safety Checklist

- [ ] `premake5 vs2026` で生成確認済み
- [ ] TeamGame startup validation を実行済み
- [ ] `projects/ParkourGame` を変更していない

## Validation

- [ ] `generateallprojects.ps1` または `premake5.exe vs2026`
- [ ] `msbuild Unnamed.slnx /m /p:Configuration=Debug /p:Platform=x64`
- [ ] `TeamGameApp --validate-startup-only`

