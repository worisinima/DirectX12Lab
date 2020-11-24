
#define PI 3.1415927f

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

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    // from spherical coordinates to cartesian coordinates    
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
    // from tangent-space vector to world-space sample vector    
	float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float a = roughness;
	float k = (a * a) / 2.0; // k_IBL
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

float2 IntegrateBEDF(float NoV, float Roughness)
{
	float3 V;

	V.x = sqrt(1 - NoV * NoV);
	V.y = 0;
	V.z = NoV;
	float A = 0;
	float B = 0;
	float3 N = { 0, 0, 1 };
	uint SAMPLE_COUNT = 1024;

	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, N, Roughness);
		float3 L = normalize(2.0 * dot(V, H) * H - V);
		float NoL = max(L.z, 0.0);
		float NoH = max(H.z, 0.0);
		float VoH = max(dot(V, H), 0.0);

		if (NoL > 0.0)
		{
			float G = GeometrySmith(N, V, L, Roughness);
			float G_Vis = (G * VoH) / (NoH * NoV);
			float Fc = pow(1.0 - VoH, 5.0);
			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);
	return float2(A, B);
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 Output = 1;
	Output.rgb = float3(IntegrateBEDF(pin.UV.xy.x, 1 - pin.UV.xy.y), 0.0f);
	return Output;
}