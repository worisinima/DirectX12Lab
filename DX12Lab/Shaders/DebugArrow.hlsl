
cbuffer cbPerObject : register(b0)
{
	float4x4 gWVP;
};

cbuffer cbPerObject2 : register(b1)
{
	float4 Color;
};

struct VertexIn
{
	float3 PosL : POSITION;
	float3 Norm : NORMAL;
	float2 Coord : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 UV : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWVP);
	vout.PosH = posW;//mul(posW, gViewProj);
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 Output = Color;
	Output.a = 1;
	return Output;
}