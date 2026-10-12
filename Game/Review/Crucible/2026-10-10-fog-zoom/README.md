# Dense Fog close-camera review fixes

The project explicitly sets `r.LocalFogVolume.GlobalStartDistance=100` in
DefaultEngine.ini. The pinned renderer's source default is 1000 cm (not the
2000 cm cited in review); an explicit near start avoids clipping the near side
of banks at close zoom. No engine source was changed.

These unmodified editor-renderer captures use the runtime camera configuration:
1200, 1600 and 2200 cm distance, -60-degree pitch, matching the gameplay-camera
FOV. Niagara is hidden throughout. Each pair changes only `r.LocalFogVolume`
between 0 and 1. The log confirms start distance 100 from SystemSettingsIni.
These validate rendering at gameplay camera geometry, not live-match input,
camera collision or packaged-client acceptance.

| Distance | Fog disabled | Persistent base enabled |
|---|---|---|
| 1200 cm | ![Disabled](MinDistance_off.png) | ![Base](MinDistance_base.png) |
| 1600 cm | ![Disabled](Distance_off.png) | ![Base](Distance_base.png) |
| 2200 cm | ![Disabled](MaxDistance_off.png) | ![Base](MaxDistance_base.png) |

`CaptureFogZoom.py` is the diagnostic capture harness, derived from the standard
Crucible capture script. Run it with UnrealEditor's `-ExecutePythonScript`,
`-EnablePlugins=PythonScriptPlugin -RenderOffscreen -NoTextureStreaming` and set
`VEYRA_REVIEW_PROFILE=low`, `VEYRA_REVIEW_VIEWS=Play_DenseFog` and
`VEYRA_REVIEW_OUTPUT` to a fresh output folder. It changes transient cameras and
visibility only and does not save the map. `manifest.json` records transforms;
`evidence.json` records image hashes.

The fallback-map automation now checks local-fog and Niagara components instead
of counting fog as static meshes. Its floor covers the authored layout so all
lobes can resolve their playable surface. The editor build and all 374 UI tests
passed (report `20261010-141813`).
