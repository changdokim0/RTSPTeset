// YUV to RGB ∫Ø»Ø «»ºø ºŒ¿Ã¥ı
Texture2D yTexture : register(t0);
Texture2D uTexture : register(t1);
Texture2D vTexture : register(t2);
SamplerState samplerState : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float y = yTexture.Sample(samplerState, input.texCoord).r;
    float u = uTexture.Sample(samplerState, input.texCoord).r - 0.5;
    float v = vTexture.Sample(samplerState, input.texCoord).r - 0.5;

    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;

    return float4(r, g, b, 1.0);
}
