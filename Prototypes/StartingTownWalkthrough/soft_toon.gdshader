shader_type spatial;
render_mode diffuse_lambert, specular_disabled, cull_disabled;

uniform vec4 surface_color : source_color = vec4(0.75, 0.75, 0.7, 1.0);
uniform float step_strength : hint_range(0.0, 1.0) = 0.3;
uniform float pattern_strength : hint_range(0.0, 1.0) = 0.0;
varying vec3 world_point;

void vertex() {
    world_point = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
    vec2 tile = world_point.xz / 1.8;
    tile.x += mod(floor(tile.y), 2.0) * 0.5;
    vec2 edge_distance = min(fract(tile), 1.0 - fract(tile));
    float joint = 1.0 - smoothstep(0.01, 0.026, min(edge_distance.x, edge_distance.y));
    ALBEDO = surface_color.rgb * (1.0 - joint * pattern_strength * 0.15);
    ROUGHNESS = 0.92;
}

void light() {
    float incident = max(dot(NORMAL, LIGHT), 0.0);
    float band = 0.16 + 0.38 * smoothstep(0.22, 0.25, incident)
                      + 0.46 * smoothstep(0.70, 0.73, incident);
    float diffuse_value = mix(incident, band, step_strength);
    DIFFUSE_LIGHT += ALBEDO * LIGHT_COLOR * ATTENUATION * diffuse_value / PI;
}

