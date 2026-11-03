# Building from source

Use Windows x64 with Visual Studio 2026 C++ tools, clang-cl, xmake, Python 3.13 with NumPy, and Node.js 22 or newer. The Windows 26.51 target uses LeviLamina 26.51.9; the 26.20 target uses 26.20.7.

```powershell
./build.ps1 -OutputDirectory C:/build/mountains-and-seas -GameLine 26.51
```

Choose a new output directory. The script builds the terrain tool and client DLL, assembles the self-contained runtime and creates `mountains-and-seas-0.4.0-Windows-MC1.26.51.01.zip`. `-GameLine 26.20` selects the other SDK and labels the ZIP with `MC1.26.20.04`.

The current source and the retained 26.20 release asset have different kernel revisions. To reproduce a published asset, first check out its exact `sourceCommit` from the release's `release.json`.

The [Windows workflow](../.github/workflows/build.yml) builds 26.51 and uploads a game-labelled artifact. Its result is separate from the checked binaries attached to a product release.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/native/`, `src/rules/` | Shared terrain, water, caves, vegetation and structure rules |
| `src/persistence/` | Structure persistence |
| `ports/windows-client/` | Windows game integration, world creation and seed service |
| `tools/`, `data/` | Seed preparation, runtime data and required resources |
| `tests/` | Rule and compatibility tests |

Python calls the Node.js seed builder and native terrain tool when preparing a world recipe. The void-world fixture contains no player data or generated chunks. Keep build intermediates, reports and personal configuration outside tracked source. See [release rules](RELEASE.md) for provenance, naming and checksums.
