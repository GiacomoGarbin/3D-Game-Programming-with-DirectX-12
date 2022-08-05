cbuffer AmbientOcclusionCB : register(b0)
{
    float4x4 gProj;
	float4x4 gProjInverse;
    float4x4 gProjTex;
	float4   gOffsetVectors[14];

    float4 gBlurWeights[3];

	float2 gRenderTargetSizeInverse;

    // coordinates given in view space
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEpsilon;

    float2 padding;
};

cbuffer RootConstantsCB : register(b1)
{
    bool gHorizontalBlur;
};

SamplerState gSamplerPointClamp  : register(s0);
SamplerState gSamplerLinearWrap  : register(s1);
SamplerState gSamplerLinearClamp : register(s2);
SamplerState gSamplerDepthMap    : register(s3);

Texture2D gNormalMap : register(t0);
Texture2D gDepthMap  : register(t1);

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};