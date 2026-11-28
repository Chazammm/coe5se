# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

## Features

- Windows players can enable batching for textured scrollbars and tooltip frames.
- Windows players can enable faster world map texture preparation and reuse prepared opaque terrain when zooming or panning.
- Content mods can benefit from an optional Windows cache for repeated numeric `+player` arguments during turn-start rule processing.
- Native graphics plugins can call `jazzyBeforeNativeDraw()` before drawing directly and `jazzyBeforeTextureUpdate(texture)` before editing a cached texture on the game render thread.

## Bugs

- World-map texture preparation checks layer transfers, preserves other mods' GL errors, releases tile snapshots when the rendering context changes and handles external army sprite bounds with MSAA enabled. Army texture copies are spread across map draws.
- Texture batching preserves rectangle textures across texture units and uses original textures when a render context moves between threads. Deferred army uploads retain reusable layers, and rejected sprites can yield cache slots to new textures.
- With native plugins loaded, texture batching checks rectangle targets and sampling state between primitives. It respects legacy shaders, per-unit LOD bias and mixed mip formats. Separate shader pipelines and flat-shaded sprites retain native drawing. Restoring a scissor preserves other viewports. Temporary render threads use the native map and scene paths so they can hand off their context safely. Unsupported contexts leave cache registrations available.

## Lua

- Owned test fields support additional stock fortifications and sampled Elemental Plane locations on Windows. `coe5.testFieldTerrainInfo` reports the loaded fort parts and availability.
