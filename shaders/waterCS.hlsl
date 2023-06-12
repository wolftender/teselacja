Texture2D<float> gPrevTexture : register(t0);
RWTexture2D<float> gOutTexture : register(u0);

static const float N = 512.0f;
static const float h = 2.0f / (N - 1.0f);
static const float c = 1.0f;
static const float dt = 1.0f / N;
static const float A = (c*c*dt*dt)/(h*h);
static const float B = 2.0f - 4.0f * A;

[numthreads(16, 16, 1)]
void main (uint3 DTid : SV_DispatchThreadID) {
	uint2 curr = DTid.xy;
	uint width, height;

	// todo: move this performance monstrosity somewhere else :)
	gOutTexture.GetDimensions(width, height);
	uint dl = curr.x;
	uint dr = width - dl;
	uint du = curr.y;
	uint db = height - du;
	uint mindist = min(dl, min(dr, min(du, db)));
	float l = float (mindist) / (2.0f * float (width)) + 0.5f;

	float d = 0.95f * min (1.0f, 3.0f * l / 0.22f);

	uint2 top = uint2 (curr.x, curr.y - 1);
	uint2 bottom = uint2 (curr.x, curr.y + 1);
	uint2 left = uint2 (curr.x - 1, curr.y);
	uint2 right = uint2 (curr.x + 1, curr.y);

	float z_0c = gOutTexture[curr].r;
	float zl = gPrevTexture[left].r;
	float zr = gPrevTexture[right].r;
	float zu = gPrevTexture[top].r;
	float zd = gPrevTexture[bottom].r;
	float zc = gPrevTexture[curr].r;

	float res = d * (A * (zl + zr + zu + zd) + B * zc - z_0c);
	//float res = 1.5f * d * (zl + zr + zu + zd + zc);
	gOutTexture[DTid.xy].r = res;
}