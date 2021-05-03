cbuffer ConstantBuffer
{
    float4x4 gWorldViewProj;
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

    vout.PositionH = mul(float4(vin.PositionL, 1.0f), gWorldViewProj);
    vout.color = vin.color;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.color;
}