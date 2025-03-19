// 파일 경로: YUVShader.hlsl
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj;
};

Texture2D yTexture : register(t0);
Texture2D uTexture : register(t1);
Texture2D vTexture : register(t2);
SamplerState samplerState : register(s0);

struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(input.Pos, WorldViewProj);
    output.Tex = input.Tex;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float y = yTexture.Sample(samplerState, input.Tex).r;
    float u = uTexture.Sample(samplerState, input.Tex).r - 0.5;
    float v = vTexture.Sample(samplerState, input.Tex).r - 0.5;

    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;

    return float4(r, g, b, 1.0);
}

technique11 Render
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));
    }
}