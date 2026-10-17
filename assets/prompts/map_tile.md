# Class template: map_tile

Text between the markers is appended to the style preamble; `{summary}` is
replaced with the slot summary.

<!-- template:begin -->
Subject: a square, seamlessly tileable surface texture of {summary}.
Requirements:
- It MUST tile seamlessly on all four sides: the left edge continues exactly
  into the right edge and the top edge continues exactly into the bottom edge,
  with no visible seam, border, frame, margin or edge darkening when repeated
  in a grid.
- Straight-on orthographic view, perfectly flat to the camera; no perspective,
  no foreshortening, no horizon, no depth of field.
- Detail, wear and colour variation are evenly distributed across the whole
  image; no single focal point, no unique feature that would stand out when
  repeated, no large stains concentrated in one area.
- Uniform brightness from corner to corner; no vignette, no gradient, no
  spotlight, no cast shadows.
- The texture fills the entire square canvas edge to edge; it is a texture
  swatch only, not a scene, not an object on a background, not a mockup.
<!-- template:end -->
