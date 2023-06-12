cbuffer cbSurfaceColor : register(b0) {
	float4 surfaceColor;
};

cbuffer cbLights : register(b1) {
	float4 lightPos;
};

Texture2D albedo : register(t0);
Texture2D normalMap : register(t1);

SamplerState samp : register(s0);

struct PSInput {
	float4 pos : SV_POSITION;
	float3 worldPos : POSITION0;
	float3 norm : NORMAL0;
	float3 viewVec : NORMAL1;
	float3 tangent : TANGENT0;
	float2 tex : TEXCOORD0;
};

static const float3 ambientColor = float3(0.2f, 0.2f, 0.2f);
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
static const float kd = 0.5, ks = 0.2f, m = 100.0f;

float3 normalmapping (float3 N, float3 T, float3 tn) {
	float3 B = normalize (cross (N, T));
	T = cross (B, N);
	float3x3 TBN = transpose (float3x3(T, B, N));

	return normalize (mul (TBN, tn));
}

float4 main (PSInput i) : SV_TARGET
{
	float3 albedoColor = albedo.Sample (samp, i.tex).xyz;
	float3 textureNormal = normalMap.Sample (samp, i.tex).xyz;

	float3 viewVec = normalize (i.viewVec);
	float3 normal = normalmapping(normalize (i.norm), normalize(i.tangent), textureNormal);

	float3 color = albedoColor.rgb * ambientColor; //ambient reflection
	float3 lightVec = normalize (lightPos.xyz - i.worldPos);
	float3 halfVec = normalize (viewVec + lightVec);
	color += lightColor * albedoColor.xyz * kd * saturate (dot (normal, lightVec)); //diffuse reflection
	color += lightColor * ks * pow (saturate (dot (normal, halfVec)), m); //specular reflection

	return float4(saturate (color), surfaceColor.a);
}