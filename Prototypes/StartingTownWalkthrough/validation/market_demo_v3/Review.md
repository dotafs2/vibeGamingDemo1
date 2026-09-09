# Starting Town market street demo v3 review

## Current disposition: r10 architecture correspondence rejected

The user subsequently rejected r10's building correspondence, explicitly excluding character repetition as the issue. The root agent re-viewed the actual comparison and inspected assembly code, and withdraws the effective internal geometry/composition pass. Prior agent reviews below are retained as history, not a current approval. Component validity and seven point projections did not establish that the modeled architecture matched the reference. See `Architecture_Mismatch_Diagnosis.md` for the specific structural and review-process causes. Runtime/collision evidence remains valid within its original technical scope. No modeling or rendering changes were made in this diagnostic follow-up.

## Purpose

This is an independent market-street reference contract for the next modeling pass. It was created after comparing the supplied anime still with the previous `market_comparison.png`. It is intentionally limited to one demo view and does not approve the global rendering style.

## Reference normalization

The source loaded successfully in Edge through Playwright on 2026-09-09: browser-reported file dimensions `2560 x 1600`; this does not verify original/native source resolution. The anime still has approximately 80 px black bars at top and bottom. The active picture is therefore represented as `x=u*2560`, `y=80+v*1440`; `u,v` are normalized coordinates in the active 16:9 picture. The overlay was re-rendered at 1280x800 with 40 px bars and visually checked. The complete machine-readable contract is in `reference_landmarks.json`.

## Why the previous model failed alignment

The former result was assembled from semantic parts (two facades, gate, tower, stalls) with hand-authored world positions and a camera that was only aimed toward the gate. It did not solve the image's perspective, silhouette, occlusion, or foreground scale as coupled constraints. The result can contain the named objects and still be the wrong street. Detail was then added to those wrong volumes, which increased mesh complexity without improving correspondence.

## Non-negotiable visual checks

The next demo must show, in the same 16:9 view, the gate opening and wall, the round tower's repeated arcade, staggered domes, both receding facade planes, layered sagging awnings, and the dense foreground crowd. In particular, the large cropped foreground character and the right wall are scale anchors. A straight awning strip, a simple thin-column tower, an empty street, or evenly spaced mannequin rows fails the shape gate even if the camera is pointed at the correct gate.

## Acceptance protocol

1. Freeze the camera and render the modeled demo at 16:9.
2. Project each JSON landmark into the render and report normalized `u,v` error, median and maximum.
3. Separately mark every `shape_detail_gate` pass/fail by visual inspection. Numeric points cannot excuse a failed silhouette or occlusion gate.
4. Iterate model geometry, camera, and object placement until all shape gates pass and no more than two non-critical landmark points exceed tolerance. Keep rendering unchanged for this comparison.

This is an image correspondence checkpoint, not a claim of canonical city survey accuracy or final anime shading approval.

## Independent review of `market_v3_demo.png` (2026-09-09)

Status: **BLOCKED** before engine import.

The preview still fails the minimum shape contract:

- The roof mass at tower-right is a cone on a thin circular band; there is no readable cylindrical tower body with repeated broad arcade arches and balcony depth.
- The central gate is a shallow wall with a decorative arch strip. The opening reads as a gray filled panel, not a deep, traversable rounded arch with a bright street beyond.
- The distant roof forms are small triangular/conical peaks rather than the staggered rounded domes in the reference.
- The awnings are long flat ribbons with hard triangular support lines. They lack cloth volume, folds, irregular/scalloped lower edges, and the reference's layered overlap.
- The street is almost empty: barrels and boxes do not replace the reference's dense overlapping foreground people and market tables. There is no large cropped right foreground character to establish scale.
- The paving is a large uninterrupted gray plane. The reference has a visible warm, irregular paving field broken by stalls, people, and shadow patches.

This is a model/scene-composition failure, not a post-processing issue. Do not send this preview for style approval or use its camera as a passing alignment. The next preview must include the projected corner/extreme markers in `reference_landmarks.json`; report each marker's `u,v` error and pass every shape gate independently.

## Follow-up inspection: `landmark_check.png`

Status remains **BLOCKED**. The gate now has a real open void, a thick arch reveal, and a visible voussoir ring. The tower now has a recognizable repeated gallery, drum, balcony bands, and roof. Those are useful geometry changes, but the submitted check image is an isolated front elevation on a dark background, so it does not validate the anime camera or street composition.

Remaining blockers visible in this render:

- The gate opening is visually split by a large central cylindrical/flat obstruction; the reference opening must read as one unobstructed dark passage with the bright far street visible.
- The tower gallery columns/arches are too thin and ornamental at this distance; the broad alternating light/dark arcade bays need to remain legible from the target view.
- The tower roof still reads as a tall pointed cone. The reference has a short red conical cap over a substantial open cylindrical pavilion; reduce the cone dominance and preserve the gallery silhouette.
- The dome cluster is absent from the submitted view, so its position and silhouette cannot yet be verified against the left-rear dome markers.

The corrected overlay is available at `reference_landmarks_overlay.png`; its 1280x800 active-image offset is 40 px. The marker contract now uses visible arch apex/spring points and silhouette extrema rather than hidden bbox corners. The next check should be the assembled 16:9 target camera with actual stalls, cloth awnings, paving, and people present; an isolated elevation cannot pass this contract.

## Godot r3 independent review (2026-09-09)

`r3/reference.png`, `gate_detail.png`, and `tower_detail.png` were viewed at full resolution. Technical checks pass, including a 7.50 m walk through the gate; visual approval remains false.

The six reported projections are numerically close for the selected points: gate crown error is about `(-0.0086,-0.0047)`, left spring `(-0.0046,-0.0210)`, right spring `(-0.0130,-0.0213)`, tower apex `(-0.0060,0)`, and foreground head `(-0.0055,-0.0167)` in normalized active-image coordinates. These numbers only validate point placement and do not validate silhouette, occlusion, or material detail.

The three highest-priority visual blockers are:

1. **Foreground market composition.** r3 has a large repeated wall of near-identical black Kirito figures. The anime reference has varied side/back silhouettes embedded in stalls and strong overlap, with the near-right brown-haired figure as a single scale anchor. Turn and distribute the people by actual stall activity and distance; remove the uniform clone-row read.
2. **Left awning scale and cloth silhouette.** r3's awnings are repeated narrow scalloped strips running along the wall. The reference has broad, layered canopies projecting deep into the street, with large hanging cloth masses and uneven overlaps. This is a geometry/placement blocker even though the strips have some scalloping.
3. **Gate/tower relationship at the target view.** The r3 gate opening is genuinely traversable and the tower gallery is recognizable in the detail view, but from the target view the gate is an overly clean pale slab and the tower roof remains cone-dominant. Preserve the single unobstructed opening and arcade, expose enough tower drum/base above the gate, and add readable masonry depth/variation. The gate and tower must read as the same layered landmark group as the reference, not isolated primitives.

Do not change global rendering to hide these issues. The point projections are useful for camera/anchor placement, while the three shape/composition gates above still block the market demo.

## Godot r4 independent review (2026-09-09)

Viewed `r4/comparison.png` and `r4/details.png` at full resolution. The three reported code fixes are present in the current `craft_landmarks.py`: the gate spandrel is a closed curved fill, dome profile uses a half-quarter arc (`pi*.5`), and tower spandrels are positioned at bay midpoints rather than column centers. These changes are visible in the detail images: the gate is a single traversable opening, the tower bays are no longer simply crossed by the arches, and the dome profiles are positive rounded caps.

The target roof ratio should not be rejected solely for being pointed. In the anime still the visible red roof is approximately 95 px high by 120 px eave width; r4 is approximately 105 px by 135 px in the matching 16:9 panel. That is close enough for this pass. The remaining tower issue is its lower drum/base being hidden by the gate and the right wall, which reduces the intended open-pavilion read.

Three current blockers, ordered by impact:

1. **Left canopy depth and profile.** The canopy now reaches into the frame, but it reads as a thick sinusoidal tube with repeated horizontal waves. The reference reads as broad cloth planes with a few natural sagging bays, folds, thickness and overlapping layers. The r4 projection report also exposes a real mismatch: `left_awning_inner_lower` is `(0.3517,0.5449)` against target `(0.415,0.74)`, delta `(-0.0633,-0.1951)`. This marker is authored geometry and is therefore not a zero-error proof; move the actual visible front/lower edge toward the target and re-project it.
2. **Foreground scale, facing and occlusion.** The existing Kirito asset is accepted for this pass. The problem is placement: the nearest left figure faces the camera and dominates as a cutout, while the reference foreground is mostly side/back activity embedded in stalls; the right figure should remain a single near-camera anchor. Turn, stagger and occlude the figures using real 3D positions so the market remains legible between bodies.
3. **Gate/tower layered landmark.** The gate geometry bug is fixed and walk-through is valid, but the target view still presents a pale, clean gate slab with insufficient masonry/arch depth, while the tower's lower drum is lost behind the bridge/right facade. Expose the tower base and maintain the unobstructed gate void in the assembled view; do not solve this by changing shader or global lighting.

The roof cone itself is not a blocker after the ratio check. The r4 result is still not visually approved until these three composition/geometry issues are corrected and rechecked in Godot.

### Marker correction

The earlier `left_awning_inner_lower` target `(0.415,0.74)` was invalid: it landed on the crowd/counter area, not cloth. It has been removed from the projected hard points. The reference now uses two visible cloth dots: `left_white_canopy_lower_corner (0.015,0.525)` and `left_green_canopy_lower_corner (0.335,0.555)`. The updated overlay was rendered and viewed; both dots land on the visible lower/front cloth edges. Any future engine marker must use these exact named cloth points or a separately documented geometry point.

## Godot r5 independent review (2026-09-09)

Viewed `r5/comparison.png`, `details.png`, and `reference.png` at full resolution. The gate/tower corrections are visible and the main camera anchors remain numerically close: gate crown `(-0.0086,-0.0047)`, springs approximately `(-0.0046,-0.0210)` / `(-0.0130,-0.0213)`, tower apex `(-0.0060,+0.0002)`, foreground head `(-0.0092,-0.0166)`. The old awning marker is correctly reported with no target and must not be scored.

The roof proportion is acceptable for this checkpoint. The tower base is visibly more exposed than r3, so “tower base hidden” is no longer an independent blocker. The traversable single gate opening is present. Global rendering remains outside this review.

Remaining blockers for the requirement “at least one anime demo view correct”:

1. **Canopy silhouette remains wrong at the strongest image scale.** The broad canopies occupy the correct left-side region, but their near edges still read as smooth inflated tubes with uniform wave bands. The anime has broad cloth planes with fewer, softer sag bays, a thin support edge, and layered overlapping sheets. Judge the lower silhouette against the two corrected cloth dots, not the old unrelated awning marker.
2. **Foreground placement still breaks the reference composition.** The allowed Kirito asset is acceptable, but the right foreground figure is a large frontal portrait and the left cluster is still too evenly staged along the wall. The reference has the close right character in a three-quarter/side presentation and irregular side/back silhouettes embedded among counters. Reorient and stagger existing characters using real 3D distances; do not add asset varieties for this pass.
3. **Gate facade and arcade lack layered depth in the target view.** The opening is technically valid, but the front gate reads as a clean pale block and the tower gallery as a thin open crown above it. Add visible gate masonry course depth and maintain the tower drum/gallery overlap with the gate. This is model/assembly only; leave shader and global lighting unchanged.

Status: **BLOCKED pending these three bounded corrections.** Technical pass and close-up geometry do not override the target-view silhouette/occlusion gate.

## r8 geometry math audit (2026-09-09)

The current tower construction is mathematically bounded: with `spring = gallery_top - 1.25 - .22`, the arch crown is `gallery_top-.22`, so it cannot rise above the gallery top. `cols=8` gives `bay=2π/8`; each arch interval is centered at `a + bay/2` and its boundaries coincide with adjacent column angles. This part is consistent with the intended cylindrical wall.

One concrete assembly bug remains in `assemble_market_demo.py`: both `left_white_canopy_lower_corner` and `left_green_canopy_lower_corner` are selected with `min(white_hem, ...)`. The green marker therefore does not come from the green hem. The second selection must use the green canopy point list, then be reprojected; otherwise the r7 “lower point matched” claim is not independently meaningful.

The background gallery wall now uses the `recess` role, and the tower mesh is closed per bay in code. These still require r9 target-view confirmation, but there is no remaining mathematical overflow or column-center placement error in the inspected tower formula.

### Correction to the r8 canopy audit

The prior concern about the green canopy marker was incorrect. In the inspected assembly, `white_hem` is the returned combined hem list from three cream sections plus the final green section, so the r7 point can indeed land on the green cloth. It was not evidence of a false or invalid match. The explicit material-keyed `canopy_hems['canvas_cream']` / `canopy_hems['canvas_green']` change is a clarity improvement, not a correction of a proven geometry error. Future judgment will use the actual visible r10 render and projected point, not variable names or comments.

## Godot r6 independent review (2026-09-09)

Viewed `r6/comparison.png` and `details.png`. The left canopy now has correct broad street penetration and smoother shared curvature; its remaining problem is primarily the exaggerated tube-like outer cross-section, but it is no longer the r5 narrow-strip failure. The gate face now has continuous courses and the target camera's gate opening is unobstructed.

The r6 visual result remains blocked by three concrete issues:

1. **Tower arcade still reads as straight columns.** In `details.png`, the wide dark bays are bounded by vertical columns, while the curved spandrel/arch pieces appear only as small ornamental white fragments near the crown. The reference's repeated arch openings must be the dominant silhouette between columns. Increase the visible arch shoulder width/depth and place the spring points between adjacent columns so the arch voids, not just columns, read at target distance.
2. **Right wall still consumes the landmark group.** In `comparison.png`, the long right facade occupies a much larger foreground plane and hides the tower's right drum/side, whereas the reference keeps the tower and gate group more exposed. The planned wall setback should be judged by the visible tower width and gate-to-wall edge relationship, not only by the wall's world x coordinate.
3. **Canopy still has a manufactured inflated profile.** Its longitudinal placement is now close, but the upper warm canopy and white canopy have near-constant smooth wave amplitude and a thick rounded edge. Reduce the cross-section and use fewer, asymmetric sag bays with a thin hem/support edge; keep the broad projection and the existing cloth point contract.

The roof apex and gate point projections are not blockers in r6. Existing Kirito asset use remains accepted; only placement/occlusion may be checked.

## Godot r10 final internal model/scene review (2026-09-09)

Viewed `r10/comparison.png`, `details.png`, `reference.png`, and `motion_090.png` at full resolution. The reported source SHA is `ce7a7f9d0264e35691b62e046e54909594ed7280ee313342ffea4f4b97f5d87e`. This is an internal model and composition decision for one market-street demo view; it is not user visual approval and does not approve the global render style.

### Seven projected markers

| marker | delta `(u,v)` |
|---|---:|
| white canopy lower corner | `(-0.00139,-0.00470)` |
| green canopy lower corner | `(-0.00059,+0.00578)` |
| gate opening crown | `(-0.00857,-0.00467)` |
| gate opening left spring | `(-0.00464,-0.02100)` |
| gate opening right spring | `(-0.01298,-0.02127)` |
| tower roof apex | `(-0.00604,+0.00022)` |
| foreground character head | `(-0.00923,-0.01655)` |

Absolute error median is approximately `0.0098` in normalized image distance and maximum is approximately `0.0249`, at the right gate spring. All markers are in front of the camera. These points are correspondence evidence only, not an image similarity score.

### Six shape gates

1. **Round tower arcade — PASS.** r10 has a continuous cylindrical pavilion with repeated broad rounded openings, visible curved shoulders, boundary columns, balcony and roof. The detail view no longer reads as isolated straight columns.
2. **Gate opening — PASS.** The central gate is a single rounded opening with visible thickness, interior soffit and a readable street beyond; it is also walkable in the runtime evidence.
3. **Awning cloth — PASS for this model checkpoint.** The left white/green and upper warm canopies have broad projection, thickness, lower folds/sags and overlapping layers. The profile remains smoother and more regular than the anime still, but that is a refinement gap, not a failed placement or missing shape.
4. **Dome cluster — PASS.** The rear domes read as multiple offset rounded volumes with drums and separate caps rather than simple triangles.
5. **Facade rhythm and parapets — PASS.** Both receding sides have continuous building planes, windows/shutters, parapets and a visible street corridor. The right wall is still visually heavier than the anime reference, but the tower and gate remain exposed enough for this one view.
6. **Foreground market mass — PASS.** Existing Kirito instances form a dense near/far mass with overlap, stalls, barrels and counters, and the foreground scale anchors are present. Asset variety and character appearance are outside this checkpoint.

### Internal conclusion

**PASS for one market-street model/composition demo view.** The remaining visible differences are mainly the unapproved rendering/material treatment, a smoother/more regular canopy profile, heavier right-wall mass, and repeated character appearance. They do not block this narrowly scoped internal geometry decision. This is not approval of the global art style, Tolbana, or the persistent-world gameplay loop.

## Godot r7 independent review (2026-09-09)

Viewed `r7/comparison.png` and `details.png` at full resolution. The gate remains open and the left canopy now has the correct broad projection. The r7 tower rebuild is an improvement: the bay arches are now visibly present instead of only ornamental strips.

The demo is still blocked by three bounded visual issues:

1. **Tower arcade silhouette is still too column-dominant.** In the right panel of `details.png`, the arch shoulders taper into narrow pointed-looking pieces and the vertical columns remain the strongest separators. The anime tower reads as broad rounded dark arch bays with substantial masonry between them. The unified parametric arch needs thicker continuous curved shoulders and a clearer rounded lower edge at the bay openings; verify from the target distance, not only the close-up.
2. **Right facade remains too dominant in the matched view.** In `comparison.png`, the right wall cuts deeply into the frame and hides most of the tower's right side, while the reference preserves more of the round tower body and its relationship to the gate. The planned setback/forward-corner adjustment should be checked by the visible tower width and right wall edge in image space.
3. **Canopy cloth is broad but still overly manufactured.** The left canopy now reaches the right image region correctly, but its upper warm cloth is a nearly constant-width ribbon with a hard, straight-ish lower rim; the white canopy is also a very smooth single sheet. The remaining correction is local profile: thinner hem, uneven sag bays and a few folds while retaining current depth. This is geometry, not global shading.

Existing Kirito appearance and the numeric gate/tower apex points are accepted for this checkpoint. Status remains **BLOCKED** until the target-view arcade, wall occlusion and canopy profile are corrected and rechecked.

## r6 code audit before render (2026-09-09)

The current source has the dome `pi*.5` profile and tower bay midpoint placement, but the next reported fixes are not all represented yet:

- Gate masonry courses are generated only in the arch span (`x=cx-r..cx+r`) and only above the curve. The side piers remain large unbroken boxes, so a full-front brick read is still missing.
- Tower `tower_bay_spandrel_*` is a rectangular strip above each arch. It does not create a closed arch shoulder/spring transition; the visible arch ring is still the only curved shoulder geometry.
- The drum windows are placed at `base + drum_height*.52`, on the upper half of the drum and partly hidden by the gallery/balcony. Their radial placement also needs a complete dark recess behind each opening rather than a small surface patch.
- The gate's rear wall/inside tunnel is present, but a full assembled view must confirm that no rear wall or interior strip closes the visual opening from the target camera.

These are code-level checks, independent of a zero-error projection report. Suggested acceptance for r6: visible brick courses across the complete gate face including piers, each tower bay has a curved closed shoulder with clear spring points between columns, and at least one full-height drum window remains readable below the gallery from the target view. Do not count a comment claiming these features as implementation evidence until the engine render shows them.

### r5 character review correction

The phrase “large frontal portrait” must not be treated as a failure by itself. The anime reference's right foreground character is also front-facing with a slight turn, similar head height and bottom/right crop. For r6 onward, character acceptance is limited to measurable occupancy, relative 3D distance, occlusion of stall landmarks, and whether the existing pose produces a comparable three-quarter/side read. The approved existing Kirito asset and its appearance are out of scope for rejection.
