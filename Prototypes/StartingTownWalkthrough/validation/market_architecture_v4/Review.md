> 后续主代理实际检查发现：本文件中的代理参考描线和“已实际确认”结论不可靠，未被组装器采用。当前模型及最终实机证据以同目录 README.md、authored_facades.json 和 r5/ 为准。此文保留失败历史。
# Market architecture v4 reference line review

## Scope

This document defines visible architectural lines for one Starting Town market-street anime view. Coordinates use the active `2560x1440` picture after removing the source's approximate 80 px top and bottom bars. They are image reconstruction constraints, not a canonical survey.

## First structural findings

The decisive structure is the coupled perspective group: stepped left near/mid building turns, a low bridge gate, and the round tower rising behind and to the right. The close right wall is a separate perspective plane whose terminating edge controls how much of the tower remains visible. Treating these as independent objects produces the previous mismatch even when each named object exists.

The line contract distinguishes visible contour points from uncertain hidden edges. It uses polygons and polylines for silhouette and window rhythm; polygon centers are never acceptance markers. The dome and lower tower points are marked medium/uncertain where the anime still is occluded.

## Facade pixel trace verification

The integer pixel trace file is `facade_traces_px.json`. Its contours were rendered over the source image at 1280x800 (source displayed at half scale with the 40 px top/bottom letterbox bars) and viewed in `facade_traces_overlay.png`. The left-mid four tall windows align with the visible sequence around source x `750..896`, y `395..588`; the right upper/lower window groups align with the visible right facade planes. The left-near lower windows and parts of the right lower contour are explicitly uncertain because stalls/characters hide them.

This overlay check is limited to the facades and windows. Central dome contours remain invalid and are excluded from the trace contract until remeasured.

### Correction after crop review

The first hand-estimated central dome polygons were wrong: the large dome in `central_landmarks.png` is approximately local `(193,256)` plus crop offset `(770,55)`, giving file position `(963,311)`. Because the crop's exact extraction convention and black-bar mapping need to be kept explicit, all central dome contours are now marked `valid_for_modeling: false` and emptied in the JSON. Root/modeler should remeasure that region directly from the supplied crop. The left/right facade and canopy contours remain the active structural guidance.

## Current r10 comparison diagnosis

Relative to these lines, the model is closest at the gate opening and roof apex. The remaining structural differences to correct or verify are the amount of right-wall occlusion, the left building's stepped depth/window grouping, the tower's visible lower drum under the gallery, and the canopy lower/upper edge relationship. Rendering and the repeated character asset are deliberately excluded from this architecture check.

## Verification rule

The next 3D pass should project mesh extrema or named line intersections into the JSON coordinate space and show the result on the original image. A marker placed on a hidden corner or a center of a broad polygon does not count. Any point labeled `uncertain` is a guide for modeling and cannot be used to claim exact alignment.
