sampler2D sdfAtlas0 : register(s12);
sampler2D sdfAtlas1 : register(s13);
sampler2D sdfAtlas2 : register(s14);
sampler2D sdfAtlas3 : register(s15);

static const float spread = 8.0f;
static const float atlasSize = 1024.0f;

struct PS_IN {
    float4 col : COLOR0;
    float2 uv0 : TEXCOORD0;
    float4 pageIdx : TEXCOORD1;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float4 main(PS_IN IN) : COLOR {
    float2 uv = IN.uv0;

    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    float d = sqrt(max(dot(dx, dx), dot(dy, dy)));

    // How many screen pixels the spread covers, kept above 1.1 to avoid ghosting.
    float pxRange = spread / max(d * atlasSize, 1e-6);
    float screenPxRange = max(pxRange, 1.1f);

    // Screen size of the 64 px design size, drives outline width and smoothing.
    float screenFontSize = 1.0f / max(d * (atlasSize / 64.0f), 1e-6);

    float outlineHint = IN.pageIdx.y;
    float outlinePx = 0.0f;

    if (outlineHint >= 1.5f) {
        outlinePx = max(2.5f, pow(screenFontSize, 0.150f) * 1.5f);
    } else if (outlineHint >= 0.5f) {
        outlinePx = max(1.25f, pow(screenFontSize, 0.075f) * 1.35f);
    }

    float smoothing = 1.0f - min(0.3f, screenFontSize * 0.0035f);
    screenPxRange *= smoothing;

    int atlasPage = int(IN.pageIdx.x + 0.5f);

    float4 sample;
    if (atlasPage == 0) {
        sample = tex2D(sdfAtlas0, uv);
    } else if (atlasPage == 1) {
        sample = tex2D(sdfAtlas1, uv);
    } else if (atlasPage == 2) {
        sample = tex2D(sdfAtlas2, uv);
    } else {
        sample = tex2D(sdfAtlas3, uv);
    }

    float sd = median(sample.r, sample.g, sample.b);

    // Small text gets a tiny bias so thin strokes stay visible.
    float bias = (screenFontSize < 12.0f) ? (12.0f - screenFontSize) * 0.003f : 0.0f;
    float opacity = saturate((sd - 0.5f + bias) * screenPxRange + 0.5f);

    if (outlinePx > 0.0f) {
        // The alpha channel holds the SDF at 5x the range, so the outline scales by 5.
        return float4(lerp(float3(0.0f, 0.0f, 0.0f), IN.col.rgb, opacity), max(opacity, saturate((sample.a - 0.5f) * screenPxRange * 5.0f + outlinePx)) * IN.col.a);
    }

    return float4(IN.col.rgb, opacity * IN.col.a);
}
