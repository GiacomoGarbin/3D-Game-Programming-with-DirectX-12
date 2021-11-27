Texture2D gBaseMap : register(t0);
Texture2D gEdgeMap : register(t1);

SamplerState gSamplerPointClamp : register(s1);

static const float2 gTexCoords[6] = 
{
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

VertexOut VS(uint VertexID : SV_VertexID)
{
	VertexOut result;
	
	result.TexCoord = gTexCoords[VertexID];
	
	// map [0,1]^2 to NDC space
	result.PositionH = float4(2.0f*result.TexCoord.x - 1.0f, 1.0f - 2.0f*result.TexCoord.y, 0.0f, 1.0f);

    return result;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 base = gBaseMap.SampleLevel(gSamplerPointClamp, pin.TexCoord, 0.0f);
	float4 edge = gEdgeMap.SampleLevel(gSamplerPointClamp, pin.TexCoord, 0.0f);

    // combines two images
	return base*edge;
}