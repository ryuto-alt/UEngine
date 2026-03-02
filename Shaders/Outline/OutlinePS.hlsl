cbuffer OutlineParams : register(b1) {
    float outlineWidth;
    float3 outlineColor;
};

float4 main() : SV_TARGET0 {
    return float4(outlineColor, 1.0f);
}
