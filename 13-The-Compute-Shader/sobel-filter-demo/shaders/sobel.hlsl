Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// approximate luminance ("brightness") from an RGB value
float GetLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void CS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    // sample the pixels in the neighborhood of this pixel
	float4 c[3][3];
	for(int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 3; ++j)
		{
			int2 xy = dispatchThreadID.xy + int2(-1 + j, -1 + i);
			c[i][j] = gInput[xy]; 
		}
	}

    // ^ optimization: store sampled values in shared memory

	// for each color channel, estimate partial x derivative using sobel scheme
	float4 dx = -1.0f*c[0][0] - 2.0f*c[1][0] - 1.0f*c[2][0] + 1.0f*c[0][2] + 2.0f*c[1][2] + 1.0f*c[2][2];

	// for each color channel, estimate partial y derivative using sobel scheme
	float4 dy = -1.0f*c[2][0] - 2.0f*c[2][1] - 1.0f*c[2][1] + 1.0f*c[0][0] + 2.0f*c[0][1] + 1.0f*c[0][2];

	// gradient is (dx, dy)
    // for each color channel, compute magnitude to get maximum rate of change
	float4 mag = sqrt(dx*dx + dy*dy);

	// make edges black, and nonedges white
	mag = 1.0f - saturate(GetLuminance(mag.rgb));

	gOutput[dispatchThreadID.xy] = mag;
}