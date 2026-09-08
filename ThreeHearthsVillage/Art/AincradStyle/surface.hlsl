// Inputs use project-prefixed names to avoid Unreal shader symbol collisions.
// No displacement: the material never changes navigation/collision geometry.
float3 hp = HearthWorldCm * 0.01;
float3 hn = abs(HearthFaceNormal);
float2 hu = hn.z > max(hn.x, hn.y) ? hp.xy : (hn.x > hn.y ? float2(hp.y, hp.z) : float2(hp.x, hp.z));
float broad = 0.5 + 0.25 * sin(hp.x * 0.31 + sin(hp.y * 0.19)) + 0.25 * sin(hp.y * 0.43 + hp.x * 0.12);
float fine = 0.5 + 0.5 * sin(hu.x * 23.0 + sin(hu.y * 17.0)) * sin(hu.y * 29.0);
float3 albedo = HearthAlbedo;
if (HearthKind < 0.5)
{
    // Meadow colour does not track altitude: a hill is not a yellow elevation heatmap.
    float cover = 0.88 + 0.20 * broad + 0.025 * fine;
    float cliff = smoothstep(0.72, 0.44, hn.z);
    float strata = 0.92 + 0.06 * sin(hp.z * 2.6 + broad);
    return lerp(albedo * cover, float3(0.25, 0.255, 0.22) * strata, cliff);
}
if (HearthKind < 1.5 || (HearthKind > 2.5 && HearthKind < 3.5))
{
    float isWall = step(2.5, HearthKind);
    float2 size = lerp(float2(0.55,0.42), float2(0.72,0.30), isWall);
    float row = floor(hu.y / size.y);
    float2 cellUV = float2(hu.x / size.x + fmod(abs(row),2.0)*0.5, hu.y / size.y);
    float2 cell = floor(cellUV);
    float2 localUV = frac(cellUV);
    float cellTone = frac(sin(dot(cell, float2(12.9898,78.233))) * 43758.5453);
    float2 edgeWidth = max(fwidth(cellUV),float2(0.015,0.015));
    float2 edge = min(localUV,1.0-localUV);
    float grout = min(smoothstep(0.012,0.012+edgeWidth.x,edge.x),smoothstep(0.02,0.02+edgeWidth.y,edge.y));
    float farFade = saturate(1.0-max(edgeWidth.x,edgeWidth.y)*3.0);
    float3 stone = albedo*(0.86+0.23*cellTone+0.04*fine);
    return lerp(albedo,lerp(albedo*0.68,stone,grout),farFade);
}
if (HearthKind < 2.5)
    return albedo * (0.94 + 0.08 * broad + 0.016 * fine);
if (HearthKind < 4.5)
{
    // Keep the source shared tile UVs. Real tile courses/eaves remain geometry.
    float tileGrain = sin(HearthUV.x*19.0)*sin(HearthUV.y*31.0);
    return albedo*(0.96+0.04*tileGrain+0.05*broad);
}
if (HearthKind < 5.5)
{
    float grain = 0.5+0.5*sin(HearthUV.x*91.0+sin(HearthUV.y*13.0)*1.6);
    return albedo*(0.88+0.17*grain+0.07*broad);
}
return albedo*(0.90+0.15*broad);
