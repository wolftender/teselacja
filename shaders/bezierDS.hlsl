struct DS_OUTPUT {
    float4 pos : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 norm : NORMAL0;
    float3 viewVec : NORMAL1;
    float3 tangent : TANGENT0;
    float2 tex : TEXCOORD0;
};

struct HS_CONTROL_POINT_OUTPUT {
	float3 vPosition : WORLDPOS;
};

struct HS_CONSTANT_DATA_OUTPUT {
	float EdgeTessFactor[4]			: SV_TessFactor;
	float InsideTessFactor[2]			: SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 16


// constant buffers
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

cbuffer cbTextureOffset : register(b3) {
    float4 texOffset;
}

cbuffer cbTessFactor : register(b4) {
    uint2 tessFactor;
}

Texture2D heightMap : register(t0);
SamplerState samp : register(s0);

float3 decasteljeu (float3 b00, float3 b01, float3 b02, float3 b03, float t) {
    float t1 = t;
    float t0 = 1.0 - t;

    float3 b10, b11, b12;
    float3 b20, b21;
    float3 b30;

    b10 = t0 * b00 + t1 * b01;
    b11 = t0 * b01 + t1 * b02;
    b12 = t0 * b02 + t1 * b03;

    b20 = t0 * b10 + t1 * b11;
    b21 = t0 * b11 + t1 * b12;

    b30 = t0 * b20 + t1 * b21;

    return b30;
}

float3 derivative (float3 b00, float3 b01, float3 b02, float3 b03, float t) {
    float t1 = t;
    float t0 = 1.0 - t;

    float3 d10 = -3.0f * b00 + 3.0f * b01;
    float3 d11 = -3.0f * b01 + 3.0f * b02;
    float3 d12 = -3.0f * b02 + 3.0f * b03;

    float3 d20 = t0 * d10 + t1 * d11;
    float3 d21 = t0 * d11 + t1 * d12;

    float3 d30 = t0 * d20 + t1 * d21;
    return d30;
}

int loc (int row, int col) {
    return (row * 4) + col;
}

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float2 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch) {
	DS_OUTPUT Output;

    float3 b00, b01, b02, b03;
    float3 b10, b11, b12, b13;
    float3 b20, b21, b22, b23;
    float3 b30, b31, b32, b33;

    b00 = patch[loc (0, 0)].vPosition;
    b01 = patch[loc (0, 1)].vPosition;
    b02 = patch[loc (0, 2)].vPosition;
    b03 = patch[loc (0, 3)].vPosition;

    b10 = patch[loc (1, 0)].vPosition;
    b11 = patch[loc (1, 1)].vPosition;
    b12 = patch[loc (1, 2)].vPosition;
    b13 = patch[loc (1, 3)].vPosition;

    b20 = patch[loc (2, 0)].vPosition;
    b21 = patch[loc (2, 1)].vPosition;
    b22 = patch[loc (2, 2)].vPosition;
    b23 = patch[loc (2, 3)].vPosition;

    b30 = patch[loc (3, 0)].vPosition;
    b31 = patch[loc (3, 1)].vPosition;
    b32 = patch[loc (3, 2)].vPosition;
    b33 = patch[loc (3, 3)].vPosition;

    float3 p0 = decasteljeu (b00, b01, b02, b03, domain.x);
    float3 p1 = decasteljeu (b10, b11, b12, b13, domain.x);
    float3 p2 = decasteljeu (b20, b21, b22, b23, domain.x);
    float3 p3 = decasteljeu (b30, b31, b32, b33, domain.x);

    float3 t0 = decasteljeu (b00, b10, b20, b30, domain.y);
    float3 t1 = decasteljeu (b01, b11, b21, b31, domain.y);
    float3 t2 = decasteljeu (b02, b12, b22, b32, domain.y);
    float3 t3 = decasteljeu (b03, b13, b23, b33, domain.y);

    Output.worldPos = decasteljeu (p0, p1, p2, p3, domain.y);

    float2 texStart = texOffset.xy;
    float2 texEnd = texStart + texOffset.zw;

    float4 camWorldPos = mul(invViewMatrix, float4(0.0f, 0.0f, 0.0f, 1.0f));

    float3 du = derivative (t0, t1, t2, t3, domain.x);
    float3 dv = derivative (p0, p1, p2, p3, domain.y);

    Output.norm = normalize (cross (dv, du));
    Output.tangent = normalize (dv);
    Output.tex.x = lerp (texStart.x, texEnd.x, domain.y);
    Output.tex.y = lerp (texStart.y, texEnd.y, domain.x);

    float height = heightMap.SampleLevel(samp, Output.tex.xy, 0).x;
    Output.worldPos += Output.norm * height * 0.08f;

    Output.viewVec = normalize (camWorldPos - Output.worldPos);
    Output.pos = mul (projMatrix, mul (viewMatrix, float4(Output.worldPos, 1.0f)));

	return Output;
}
