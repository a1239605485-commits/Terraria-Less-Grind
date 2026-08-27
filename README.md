# 轻松泰拉 / Terraria Less Grind v1.0.0

适用于 TEF KernelLoader 的轻量原生模组工程（目标游戏版本 1.4.4.0–1.4.5.6）。

## 功能

- 常见建筑材料的原版合成产出 ×3。
- 常用药水的原版合成产出 ×5，减少重复点击制作。
- Boss 召唤物配方中数量大于 1 的材料减半（向上取整）。
- 稀有材料保底掉落：腐肉、脊椎骨、晶状体、毒刺。

不修改武器伤害、防御、Boss 生命值、Boss 掉落或角色属性。

## 构建

直接将本工程上传到 GitHub 仓库，打开 **Actions → Build Terraria Less Grind → Run workflow**。构建完成后，在该次运行页面下载 `LessGrind.android.arm64.so` 工件；下载内容就是可直接使用的 `LessGrind.android.arm64.so` 文件，不会再打包为 `.tefpkg`。

如需本地构建，可使用 Android NDK r28：

```text
cmake --preset android-arm64-release
cmake --build --preset android-arm64-release
```

构建产物为 `LessGrind.android.arm64.so`。GitHub Actions 会在编译结束后自动定位 CMake 实际生成的 `.so`，统一输出为此文件名。

首次测试时，请查看日志中是否包含：

```text
LessGrind: Recipe hook: ready
LessGrind: Drop hook: ready
LessGrind: Recipes updated:
```
