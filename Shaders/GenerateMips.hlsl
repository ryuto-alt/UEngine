// GPU Mipmap Generation Compute Shader
// Based on DirectXTK12 implementation

SamplerState BilinearClamp : register(s0);
Texture2D<float4> SrcMip   : register(t0);
RWTexture2D<float4> OutMip : register(u0);

cbuffer MipConstants : register(b0)
{
    float2 InvOutTexelSize; // 1.0 / output dimension
    uint SrcMipIndex;
    uint Padding;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 uv = (DTid.xy + 0.5) * InvOutTexelSize;
    OutMip[DTid.xy] = SrcMip.SampleLevel(BilinearClamp, uv, SrcMipIndex);
}
