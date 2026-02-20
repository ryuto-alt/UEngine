// X-ray pass: colored silhouette through walls (enemy sense ability)
// Uses additive blending for a haze/mist feel

cbuffer XRayData : register(b0) {
    float4 xrayColor; // RGB = color, A = fade alpha
};

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(float4 position : SV_Position) {
    PSOutput output;
    output.color = xrayColor;
    return output;
}
