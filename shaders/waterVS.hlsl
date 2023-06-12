cbuffer cbWorld : register(b0) //Vertex Shader constant buffer slot 0
{
	matrix worldMatrix;
};

cbuffer cbView : register(b1) //Vertex Shader constant buffer slot 1
{
	matrix viewMatrix;
	matrix invViewMatrix;
};

cbuffer cbProj : register(b2) //Vertex Shader constant buffer slot 2
{
	matrix projMatrix;
};

struct VSInput {
	float3 pos : POSITION;
	float3 norm : NORMAL0;
};

struct PSInput {
	float4 pos : SV_POSITION;
	float3 localPos : POSITION0;
	float3 worldPos : POSITION1;
	float3 viewPos : POSITION2;
	float3 worldNorm : NORMAL0;
	float3 worldTangent : TANGENT0;
	float3 viewVec : TEXCOORD0;
};

PSInput main (VSInput i) {
	PSInput o;
	o.localPos = i.pos;

	o.worldPos = mul (worldMatrix, float4(o.localPos, 1.0f)).xyz;
	o.pos = mul (viewMatrix, float4(o.worldPos, 1.0f));
	o.viewPos = o.pos.xyz;
	o.pos = mul (projMatrix, o.pos);

	o.worldNorm = mul (worldMatrix, float4(0.0, 0.0, -1.0, 0.0)).xyz;
	o.worldNorm = normalize (o.worldNorm);
	o.worldTangent = mul (worldMatrix, float4(1.0, 0.0, 0.0, 0.0));

	float3 camPos = mul (invViewMatrix, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	o.viewVec = camPos - o.worldPos;

	return o;
}