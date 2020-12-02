//bmptex.get()漫反射,  mShadowMap , mIBLBRDFTarget预积分 , normTex.get()法线 ,cubeTex.get() 立方体贴图
Texture2D gTextures[4] : register(t0);
TextureCube gCubeMap : register(t4);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);


#define PI 3.1415927

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
};

cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float4x4 gShadowTransform;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
};

#if SKINNED

cbuffer cbSkinned : register(b2)
{
	float4x4 gBoneTransforms[96];
};

cbuffer cbMaterial : register(b3)
{
	float4 cbBaseColor;
	float cbRoughness;
	float cbMetallic;
};

cbuffer cbLight : register(b4)
{
	float4 cbLightColorAndStrenth[4];
	float4 mLightPosAndRadius[4];
};

#else

cbuffer cbMaterial : register(b2)
{
	float4 cbBaseColor;
	float cbRoughness;
	float cbMetallic;
};

cbuffer cbLight : register(b3)
{
	float4 cbLightColorAndStrenth[4];
	float4 mLightPosAndRadius[4];
};

#endif

struct VertexIn
{
	float3 PosL : POSITION;
	float3 Norm : NORMAL;
	float3 Tangent : TANGENT;
	float2 Coord : TEXCOORD;
#if SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices  : BONEINDICES;
#endif
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 ShadowPosH : POSITION0;
	float3 Norm : NORMAL;
	float3 TangentW : TANGENT;
	float3 WorldPos : POSITIONT1;
	float2 Coord : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
#if SKINNED
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
	
	float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 4; ++i)
    {
        // Assume no nonuniform scaling when transforming normals, so 
        // that we do not have to use the inverse-transpose.

        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(vin.Norm, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
        tangentL += weights[i] * mul(vin.Tangent.xyz, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
    }

    vin.PosL = posL;
    vin.Norm = normalL;
    vin.Tangent.xyz = tangentL;
	
#endif
	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	
	vout.WorldPos = posW.xyz;
	
	vout.PosH = mul(posW, gViewProj);
	
	// Just pass vertex color into the pixel shader.
	vout.Norm = mul(float4(vin.Norm, 0), gWorld).xyz;
	vout.TangentW = mul(normalize(vin.Tangent), (float3x3)gWorld);
	vout.Coord = vin.Coord;
	vout.ShadowPosH = mul(posW, gShadowTransform);
	
	return vout;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
	float3 N = normalize(unitNormalW);
	float3 T = normalize(tangentW - dot(tangentW, N) * N);
	float3 B = normalize(cross(N, T));

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
	shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
	float depth = shadowPosH.z;

	uint width, height, numMips;
	gTextures[1].GetDimensions(0, width, height, numMips);

    // Texel size.
	float dx = 1.0f / (float) width;

	float percentLit = 0.0f;
	const float2 offsets[9] =
	{
		float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
	};

    [unroll]
	for (int i = 0; i < 9; ++i)
	{
		percentLit += gTextures[1].SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
	}
    
	return percentLit / 9.0f;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / max(denom, 0.000001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX(float a2, float NoH)
{
	float d = (NoH * a2 - NoH) * NoH + 1; // 2 mad
	return a2 / (PI * d * d); // 4 mul, 1 rcp
}

// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJoint(float a2, float NoV, float NoL)
{
	float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
	float a = sqrt(a2);
	float Vis_SmithV = NoL * (NoV * (1 - a) + a);
	float Vis_SmithL = NoV * (NoL * (1 - a) + a);
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

float3 FSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 Diffuse_Lambert(float3 DiffuseColor)
{
	return DiffuseColor * (1 / PI);
}

float Pow5(float val)
{
	return val * val * val * val * val;
}

// [Burley 2012, "Physically-Based Shading at Disney"]
float3 Diffuse_Burley(float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH)
{
	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
	float FdV = 1 + (FD90 - 1) * Pow5(1 - NoV);
	float FdL = 1 + (FD90 - 1) * Pow5(1 - NoL);
	return DiffuseColor * ((1 / PI) * FdV * FdL);
}

// [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
float3 Diffuse_Gotanda(float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH)
{
	float a = Roughness * Roughness;
	float a2 = a * a;
	float F0 = 0.04;
	float VoL = 2 * VoH * VoH - 1; // double angle identity
	float Cosri = VoL - NoV * NoL;
#if 1
	float a2_13 = a2 + 1.36053;
	float Fr = (1 - (0.542026 * a2 + 0.303573 * a) / a2_13) * (1 - pow(1 - NoV, 5 - 4 * a2) / a2_13) * ((-0.733996 * a2 * a + 1.50912 * a2 - 1.16402 * a) * pow(1 - NoV, 1 + rcp(39 * a2 * a2 + 1)) + 1);
	//float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1 );
	float Lm = (max(1 - 2 * a, 0) * (1 - Pow5(1 - NoL)) + min(2 * a, 1)) * (1 - 0.5 * a * (NoL - 1)) * NoL;
	float Vd = (a2 / ((a2 + 0.09) * (1.31072 + 0.995584 * NoV))) * (1 - pow(1 - NoL, (1 - 0.3726732 * NoV * NoV) / (0.188566 + 0.38841 * NoV)));
	float Bp = Cosri < 0 ? 1.4 * NoV * NoL * Cosri : Cosri;
	float Lr = (21.0 / 20.0) * (1 - F0) * (Fr * Lm + Vd + Bp);
	return DiffuseColor / PI * Lr;
#else
	float a2_13 = a2 + 1.36053;
	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a + 0.5*a * NoL );
	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) );
	float Bp = Cosri < 0 ? 1.4 * NoV * Cosri : Cosri / max( NoL, 1e-8 );
	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
	return DiffuseColor / PI * Lr;
#endif
}

float3 EnvBRDFApprox(float3 SpecularColor, float Roughness, float NoV)
{
	// [ Lazarov 2013, "Getting More Physical in Call of Duty: Black Ops II" ]
	// Adaptation to fit our G term.
	const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
	const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
	float4 r = Roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	float2 AB = float2(-1.04, 1.04) * a004 + r.zw;

	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	// Note: this is needed for the 'specular' show flag to work, since it uses a SpecularColor of 0
	AB.y *= clamp(50.0 * SpecularColor.g, 0, 1);

	return SpecularColor * AB.x + AB.y;
}

float RandFast(uint2 PixelPos, float Magic = 3571.0)
{
	float2 Random2 = (1.0 / 4320.0) * PixelPos + float2(0.25, 0.0);
	float Random = frac(dot(Random2 * Random2, Magic));
	Random = frac(Random * Random * (2 * Magic));
	return Random;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 Output = 0.0f;
	
	float4 normalMapSample = gTextures[3].Sample(gsamAnisotropicWrap, pin.Coord * 5);
	float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.Norm, pin.TangentW);
	
	float4 BaseColor = gTextures[0].Sample(gsamAnisotropicWrap, pin.Coord);
	BaseColor.rgb *= cbBaseColor.rgb;
	
	float Roughness = cbRoughness;
	float Metallic = cbMetallic;
	float F0 = 0.04f;
	float AO = 1.0f;
	F0 = lerp(F0.rrr, BaseColor.rgb, Metallic);

	float Shadow = CalcShadowFactor(pin.ShadowPosH);
	
	for (int i = 0; i < 4; i++)
	{
		float3 PointLightPos = mLightPosAndRadius[i].xyz;
		float LightRadius = mLightPosAndRadius[i].w;
		float LightStrenth = cbLightColorAndStrenth[i].w;
		float3 LightColor = cbLightColorAndStrenth[i].rgb;
		
		float3 WPos = pin.WorldPos;
		float FallOff = distance(PointLightPos, WPos);
		FallOff = LightRadius / (FallOff * FallOff);
		
		float3 V = normalize(gEyePosW - pin.WorldPos);
		//float3 N = bumpedNormalW;
		float3 N = normalize(pin.Norm);
		//float3 L = float3(-0.8, 1, 0.5);
		float3 L = normalize(PointLightPos - WPos);
		float3 H = normalize(V + L);
		float3 R = -reflect(V, N);
	
		float NoL = saturate(dot(L, N));
		float NoH = saturate(dot(N, H));
		float NoV = saturate(dot(N, V));
		float VoH = saturate(dot(V, H));
		float NoR = saturate(dot(N, R));
	
		float3 Diffuse = Diffuse_Burley(BaseColor.rgb, Roughness, NoV, NoL, NoH);
	
		float a2 = Roughness * Roughness * Roughness * Roughness;
		float D = D_GGX(a2, NoH);
		float G = Vis_SmithJointApprox(a2, NoV, NoL);
		float F = FSchlick(VoH, F0);
	
		float3 Specular = D * G * F;

		Output.rgb += (Diffuse + Specular) * NoL * Shadow * (FallOff * LightStrenth * LightColor);

	}
	
	//float LevelFrom1x1 = 1 - 1.2 * log2(Roughness);
	//float lod = 11 - 1 - LevelFrom1x1;
	
	//float4 reflectionColor = gCubeMap.SampleLevel(gsamAnisotropicClamp, R, lod);
	////reflectionColor.rgb = pow(reflectionColor, 1.0f / 2.2f);
	
	//float2 IBLBRDF = gTextures[2].Sample(gsamPointClamp, float2(NoV, Roughness)).rg;
	
	//float3 indirectSpecular = reflectionColor.rgb * (F * IBLBRDF.x + IBLBRDF.y);

	////Temp
	//Output.rgb += 0.03f * BaseColor.rgb * AO;
	//Output.rgb += reflectionColor.rgb * 0.15f;
	
	return Output;
}