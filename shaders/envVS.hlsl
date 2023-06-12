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

struct VSOutput {
	float4 pos : SV_POSITION;
	float3 tex : TEXCOORD0;
};

VSOutput main (VSInput i) {
	VSOutput o;
	o.tex = normalize (i.pos);
	o.pos = mul (projMatrix, mul (viewMatrix, mul (worldMatrix, float4(i.pos, 1.0f))));

	return o;
}