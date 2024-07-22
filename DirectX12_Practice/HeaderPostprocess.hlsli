float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy, float4 rect)
{
    float4 ret = tex.Sample(smp, uv);
    float4 blurColor = float4(0, 0, 0, 0);

    float weights[5][5] = {
        {1 / 273.0,  4 / 273.0,  7 / 273.0,  4 / 273.0, 1 / 273.0},
        {4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0},
        {7 / 273.0, 26 / 273.0, 41 / 273.0, 26 / 273.0, 7 / 273.0},
        {4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0},
        {1 / 273.0,  4 / 273.0,  7 / 273.0,  4 / 273.0, 1 / 273.0}
    };

    float offsets[5] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            float2 offset = float2(offsets[i] * dx, offsets[j] * dy);
            float2 sampleUV = uv + offset;

            sampleUV.x = clamp(sampleUV.x, rect.x + dx * 0.5, rect.z - dx * 0.5);
            sampleUV.y = clamp(sampleUV.y, rect.y + dy * 0.5, rect.w - dy * 0.5);

            blurColor += tex.Sample(smp, sampleUV) * weights[i][j];
        }
    }

    return float4(blurColor.rgb, ret.a);
}