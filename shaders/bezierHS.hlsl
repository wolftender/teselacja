struct VS_CONTROL_POINT_OUTPUT {
	float3 vPosition : WORLDPOS;
};

struct HS_CONTROL_POINT_OUTPUT {
	float3 vPosition : WORLDPOS; 
};

struct HS_CONSTANT_DATA_OUTPUT {
	float EdgeTessFactor[4]			: SV_TessFactor;
	float InsideTessFactor[2]			: SV_InsideTessFactor;
};

cbuffer cbTessFactor : register(b0) {
	uint2 tessFactor;
}

#define NUM_CONTROL_POINTS 16

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID) {
	HS_CONSTANT_DATA_OUTPUT Output;

	Output.EdgeTessFactor[0] = 
		Output.EdgeTessFactor[1] = 
		Output.EdgeTessFactor[2] = 
		Output.EdgeTessFactor[3] = tessFactor.x;
	
	Output.InsideTessFactor[0] = 
		Output.InsideTessFactor[1] = tessFactor.y;

	return Output;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID) {
	HS_CONTROL_POINT_OUTPUT Output;

	Output.vPosition = ip[i].vPosition;
	return Output;
}
