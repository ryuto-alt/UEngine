// PBR Vertex Shader

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
    matrix lightViewProj;        // directional shadow
    matrix spotLightViewProj[4]; // spot shadows
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VSOutput {
    float4 position      : SV_POSITION;
    float3 worldPos      : POSITION0;
    float3 normal        : NORMAL;
    float2 uv            : TEXCOORD0;
    float4 shadowPos     : TEXCOORD1;
    float4 spotShadowPos0 : TEXCOORD2;
    float4 spotShadowPos1 : TEXCOORD3;
    float4 spotShadowPos2 : TEXCOORD4;
    float4 spotShadowPos3 : TEXCOORD5;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4 worldPos    = mul(float4(input.position, 1.0f), world);
    output.worldPos    = worldPos.xyz;
    output.position    = mul(float4(input.position, 1.0f), mvp);
    output.normal      = normalize(mul(input.normal, (float3x3)world));
    output.uv          = input.uv;
    output.shadowPos   = mul(worldPos, lightViewProj);

    output.spotShadowPos0 = mul(worldPos, spotLightViewProj[0]);
    output.spotShadowPos1 = mul(worldPos, spotLightViewProj[1]);
    output.spotShadowPos2 = mul(worldPos, spotLightViewProj[2]);
    output.spotShadowPos3 = mul(worldPos, spotLightViewProj[3]);

    return output;
}
