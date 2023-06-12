Texture2D bumpMap : register(t0);
textureCUBE envMap : register(t1);
SamplerState samp : register(s0);

cbuffer cbSurfaceColor : register(b0) {
	float4 surfaceColor;
};

cbuffer cbLights : register(b1) {
	float4 lightPos;
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

static const float2 size = float2 (2.0, 0.0);
static const int3 offset = int3 (-1, 0, 1);
static const float multiplier = 4.0f;

// phong paramaters
static const float3 ambientColor = float3(0.2f, 0.2f, 0.2f);
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
static const float kd = 0.5, ks = 0.2f, m = 100.0f;

// phong shading
float3 phong (float3 normal, float3 viewVec, float3 worldPos) {
	float3 color = surfaceColor.rgb * ambientColor; //ambient reflection
	float3 lightVec = normalize (lightPos.xyz - worldPos);
	float3 halfVec = normalize (viewVec + lightVec);
	color += lightColor * surfaceColor.xyz * kd * saturate (dot (normal, lightVec)); //diffuse reflection
	color += lightColor * ks * pow (saturate (dot (normal, halfVec)), m); //specular reflection
	return saturate (color);
}

// normal mapping
float3 normalmapping (float3 N, float3 T, float3 tn) {
	float3 B = normalize (cross (N, T));
	T = cross (B, N);
	float3x3 TBN = transpose (float3x3(T, B, N));

	return normalize (mul (TBN, tn));
}

// water reflections and refractions
float3 intersectRay (float3 P, float3 R) {
	float3 t = max ((-1 - P) / R, (1 - P) / R);
	float tmin = min (t.x, min (t.y, t.z));

	return P + R * tmin;
}

float fresnel (float3 N, float3 V) {
	float F0 = 0.17f;
	float cosine = max (0.0f, dot (N, V));
	return F0 + (1 - F0) * pow (1 - cosine, 5);
}

float4 main(PSInput i) : SV_TARGET {
	float2 texCoord = 0.5f * (2.0f * i.localPos.xy + 1.0f);

	// this code converts the bump map to normal map
	// it takes neighboring heights and constructs normal vectors using cross products
	float s11 = bumpMap.Sample (samp, texCoord).r;
	float s01 = bumpMap.Sample (samp, texCoord, offset.xy).r;
	float s21 = bumpMap.Sample (samp, texCoord, offset.zy).r;
	float s10 = bumpMap.Sample (samp, texCoord, offset.yx).r;
	float s12 = bumpMap.Sample (samp, texCoord, offset.yz).r;

	float3 va = normalize (float3 (size.xy, multiplier * (s21 - s01)));
	float3 vb = normalize (float3 (size.yx, multiplier * (s12 - s10)));
	float3 tn = normalize (cross (va, vb));
	float3 normal = normalmapping (i.worldNorm, i.worldTangent, tn);

	// this code performs water reflections and refractions
	float n1 = 1.0f;
	float n2 = 4.0f / 3.0f;

	float3 reflected;
	float3 refracted;
	float3 viewVec = normalize(i.viewVec);

	float d = dot (normal, viewVec);
	if (d >= 0.0f) {
		refracted = refract (-viewVec, normal, n1 / n2);
	} else {
		normal = -normal;
		refracted = refract (-viewVec, normal, n2 / n1);

		// this has to be added, tested on nvidia driver 531.68
		if (length (refracted) == 0.0f) {
			refracted = reflect (-viewVec, normal);
		}
	}

	//return float4(i.worldPos, 1.0f);

	reflected = reflect (-viewVec, normal);

	// we have to undo the scaling, this is why 15.0f is here!!!!!
	float3 reflectedColor = envMap.Sample (samp, intersectRay (i.worldPos / 15.0f, reflected));
	float3 refractedColor = envMap.Sample (samp, intersectRay (i.worldPos / 15.0f, refracted));

	float f = fresnel (normal, viewVec);
	float3 color = f * reflectedColor + (1.0f - f) * refractedColor;

	//color = pow (color, 0.4545f);
	return float4(color, 1.0f);

	// return color
	//return float4(phong(normal, normalize(i.viewVec), i.worldPos), 1.0f);
}