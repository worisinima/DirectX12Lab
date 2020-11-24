
Texture2D gTextureResource : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexIn
{
	float4 PosL : POSITION;
	float4 Coord : TEXCOORD;
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
	vout.PosH = vin.PosL;
	
	// Just pass vertex color into the pixel shader.
	vout.UV = vin.Coord;
    
	return vout;
}

float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

half3 LinearTo709Branchless(half3 lin)
{
	lin = max(6.10352e-5, lin); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	return min(lin * 4.5, pow(max(lin, 0.018), 0.45) * 1.099 - 0.099);
}

half3 LinearToSrgbBranchless(half3 lin)
{
	lin = max(6.10352e-5, lin); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	return min(lin * 12.92, pow(max(lin, 0.00313067), 1.0 / 2.4) * 1.055 - 0.055);
    // Possible that mobile GPUs might have native pow() function?
    //return min(lin * 12.92, exp2(log2(max(lin, 0.00313067)) * (1.0/2.4) + log2(1.055)) - 0.055);
}

half LinearToSrgbBranchingChannel(half lin)
{
	if (lin < 0.00313067)
		return lin * 12.92;
	return pow(lin, (1.0 / 2.4)) * 1.055 - 0.055;
}

half3 LinearToSrgbBranching(half3 lin)
{
	return half3(
                LinearToSrgbBranchingChannel(lin.r),
                LinearToSrgbBranchingChannel(lin.g),
                LinearToSrgbBranchingChannel(lin.b));
}

half3 LinearToSrgb(half3 lin)
{
    // Adreno devices(Nexus5) with Android 4.4.2 do not handle branching version well, so always use branchless on Mobile
	return LinearToSrgbBranchless(lin);
}

half3 sRGBToLinear(half3 Color)
{
	Color = max(6.10352e-5, Color); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	return Color > 0.04045 ? pow(Color * (1.0 / 1.055) + 0.0521327, 2.4) : Color * (1.0 / 12.92);
}

float3 ReinhardToneMapping(float3 color, float adapted_lum)
{
	const float MIDDLE_GREY = 1;
	color *= MIDDLE_GREY / adapted_lum;
	return color / (1.0f + color);
}

float3 ACESToneMapping(float3 color, float adapted_lum)
{
	const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;

	color *= adapted_lum;
	
	return (color * (A * color + B)) / (color * (C * color + D) + E);
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 Output = 1;
#if 1
	Output.rgba = gTextureResource.Sample(gsamLinearWrap, pin.UV.xy);
	
	Output.rgb = ACESToneMapping(Output.rgb, 1);
	//Output.rgb = LinearToSrgb(Output.rgb);
	Output.rgb = pow(Output.rgb, 1.0f / 2.2f);
#else
	Output.rgba = pow(gTextureResource.Sample(gsamLinearWrap, pin.UV.xy).rrrr * 0.99, 128);
#endif
	return Output;
}

float4 PSWithOutTone(VertexOut pin) : SV_Target
{
	float4 Output = 1;
	Output.rgba = gTextureResource.Sample(gsamLinearWrap, pin.UV.xy);
	return Output;
}