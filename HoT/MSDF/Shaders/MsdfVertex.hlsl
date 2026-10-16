float4x4 WorldViewProj : register(c0);

struct VS_IN {
    float4 pos : POSITION0;
    float4 col : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 glyph : TEXCOORD1;
};

struct VS_OUT {
    float4 hpos : POSITION;
    float4 col : COLOR0;
    float2 uv0 : TEXCOORD0;
    float4 pageIdx : TEXCOORD1;
};

// TEXCOORD1 carries the atlas page in x and the outline hint in y.
VS_OUT main(VS_IN IN) {
    VS_OUT OUT;
    OUT.hpos = mul(IN.pos, WorldViewProj);
    OUT.col = IN.col;
    OUT.uv0 = IN.uv0;
    OUT.pageIdx = float4(IN.glyph.x, IN.glyph.y, 0.0f, 0.0f);
    return OUT;
}
