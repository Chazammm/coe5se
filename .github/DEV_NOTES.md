# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

Features
- Windows players can enable batching for textured scrollbars and tooltip frames.
- Windows players can enable faster world map texture preparation and reuse prepared opaque terrain when zooming or panning.
- Content mods can benefit from an optional Windows cache for repeated numeric `+player` arguments during turn-start rule processing.

Bugs
- World-map texture preparation checks layer transfers, preserves other mods' GL errors, releases tile snapshots when the rendering context changes and handles external army sprite bounds with MSAA enabled.
