cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
};

cbuffer MainPassCB : register(b1)
{
	float4x4 gView;
	float4x4 gViewInverse;
	float4x4 gProj;
	float4x4 gProjInverse;
	float4x4 gViewProj;
	float4x4 gViewProjInverse;
	float3 gEyePositionWorld;
	float padding;
	float2 gRenderTargetSize;
	float2 gRenderTargetSizeInverse;
	float gNearPlane;
	float gFarPlane;
	float gDeltaTime;
	float gTotalTime;
};

struct VertexIn
{
    float3 PositionL : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
    float4 PositionH : SV_POSITION;
    float4 color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

	float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	//float4 PositionW = float4(vin.PositionL, 1.0f);
    vout.PositionH = mul(PositionW, gViewProj);

    vout.color = vin.color;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.color;
}