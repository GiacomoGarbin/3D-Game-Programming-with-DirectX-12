
cbuffer BlurSettingsCB : register(b0)
{
	int gRadius;
    
    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float wA;
};

Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

#define GroupThreadSize 256
#define MaxBlurRadius 5
#define CacheSize (GroupThreadSize + 2 * MaxBlurRadius)

groupshared float4 gCache[CacheSize];

[numthreads(GroupThreadSize, 1, 1)]
void HorzBlurCS(int3 GroupThreadID : SV_GroupThreadID,
                int3 DispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, wA };

    if (GroupThreadID.x < gRadius)
    {
        uint x = max(DispatchThreadID.x - gRadius, 0);
        gCache[GroupThreadID.x] = gInput[uint2(x, DispatchThreadID.y)];
    }

    if (GroupThreadID.x >= (GroupThreadSize - gRadius))
    {
        uint x = min(DispatchThreadID.x + gRadius, gInput.Length.x - 1);
        gCache[GroupThreadID.x + 2 * gRadius] = gInput[uint2(x, DispatchThreadID.y)];
    }

    gCache[GroupThreadID.x + gRadius] = gInput[min(DispatchThreadID.xy, gInput.Length.xy - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 result = 0;

    for (int i = -gRadius; i <= +gRadius; ++i)
    {
        uint j = gRadius + i;
        uint k = GroupThreadID.x + j;
        result += weights[j] * gCache[k];
    }

    gOutput[DispatchThreadID.xy] = result;
}

[numthreads(1, GroupThreadSize, 1)]
void VertBlurCS(int3 GroupThreadID : SV_GroupThreadID,
                int3 DispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, wA };

    if (GroupThreadID.y < gRadius)
    {
        uint y = max(DispatchThreadID.y - gRadius, 0);
        gCache[GroupThreadID.y] = gInput[uint2(DispatchThreadID.x, y)];
    }

    if (GroupThreadID.y >= (GroupThreadSize - gRadius))
    {
        uint y = min(DispatchThreadID.y + gRadius, gInput.Length.y - 1);
        gCache[GroupThreadID.y + 2 * gRadius] = gInput[uint2(DispatchThreadID.x, y)];
    }

    gCache[GroupThreadID.y + gRadius] = gInput[min(DispatchThreadID.xy, gInput.Length.xy - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 result = 0;

    for (int i = -gRadius; i <= +gRadius; ++i)
    {
        uint j = gRadius + i;
        uint k = GroupThreadID.y + j;
        result += weights[j] * gCache[k];
    }

    gOutput[DispatchThreadID.xy] = result;
}