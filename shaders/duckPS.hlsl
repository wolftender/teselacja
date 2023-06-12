Texture2D albedo : register(t0);
SamplerState samp : register(s0);

cbuffer cbSurfaceColor : register(b0) {
	float4 surfaceColor;
};

cbuffer cbLights : register(b1) {
	float4 lightPos;
};

struct PSInput {
	float4 pos : SV_POSITION;
	float3 worldPos : POSITION0;
	float3 norm : NORMAL0;
	float3 viewVec : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float3 front : NORMAL1;
};

static const float3 ambientColor = float3(0.4f, 0.4f, 0.4f);
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
static const float kd = 0.6f, ks = 4.0f, m = 100.0f;
static const float PI = 3.14159265359f;

float ward (float3 lightDir, float3 viewDir, float3 normal, float3 front, float shinyParallel, float shinyPerpendicular) {
	float NdotL = dot (normal, lightDir);
	float NdotR = dot (normal, viewDir);

	if (NdotL < 0.0 || NdotR < 0.0) {
		return 0.0;
	}

	//float3 e = float3 (1.0f, 0.0f, 0.0f);
	float3 H = normalize (lightDir + viewDir);
	float3 left = normalize (cross (normal, front));
	float3 forward = normalize (cross (left, normal));

	float NdotH = dot (normal, H);
	float XdotH = dot (left, H);
	float YdotH = dot (forward, H);

	float coeff = sqrt (NdotL / NdotR) / (4.0 * PI * shinyParallel * shinyPerpendicular);
	float theta = (pow (XdotH / shinyParallel, 2.0) + pow (YdotH / shinyPerpendicular, 2.0)) / (1.0 + NdotH);

	return coeff * exp (-2.0 * theta);
}

// phong shading
float3 phong (float3 fragColor, float3 normal, float3 front, float3 viewVec, float3 worldPos) {
	float3 color = fragColor.rgb * ambientColor; //ambient reflection
	float3 lightVec = normalize (lightPos.xyz - worldPos);

	// diffuse lighting
	color += lightColor * fragColor.xyz * kd * saturate (dot (normal, lightVec));

	// specular phong
	//float3 halfVec = normalize (viewVec + lightVec);
	//color += lightColor * ks * pow (saturate (dot (normal, halfVec)), m); //specular reflection
	
	// specular ward
	float wardCoeff = ward (lightVec, viewVec, normal, front, 0.8f, 0.25f);
	color += lightColor * wardCoeff * ks;

	return saturate (color);
}

float4 main (PSInput i) : SV_TARGET {
	float3 color = albedo.Sample (samp, i.uv).xyz;
	float3 phongColor = phong (color, i.norm, i.front, i.viewVec, i.worldPos);

	return float4 (phongColor, 1.0f);
}