textureCUBE envMap : register(t0);
SamplerState samp : register(s0);

struct VSOutput {
	float4 pos : SV_POSITION;
	float3 tex : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET {
	float3 color = envMap.Sample (samp, i.tex).rgb;
	//color = pow (color, 0.4545f);

	return float4(color, 1.0f);
}