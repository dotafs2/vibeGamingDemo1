# Integrated production art snapshot

The assets in this directory were copied on 2026-08-10 from the sibling task workspaces for the same repository and integrated non-destructively on `agent/art-direction`.

- `gate/surface-facility-shared.png`: user-provided 1932×814 surface-facility panorama, used only as the editor scene background; editor components remain separate.
- `elevator/`: task 2, mapped around shaft axis `X=18.25`.
- `lab/`: task 3, authored for `X=18.1…64.1`; `*-longscroll.png` files are retained masters and the far/mid/near bands are runtime inputs.
- `boss-arena/`: task 4, authored for `X=60.75…116.25` at `18.5u` height.

Do not resize or recompress these files without updating the editor scene-art mapping and/or `src/art-direction.js`, `docs/ART_DIRECTION_AND_STITCHING.md`, and `tests/art-direction-contract.test.mjs` together.
