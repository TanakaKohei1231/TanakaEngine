#include "Object3d.hlsli"


static const int LIGHTING_NONE = 0;
static const int LIGHTING_LAMBERT = 1;
static const int LIGHTING_HALF_LAMBERT = 2;

struct Material
{
    float32_t4 color;
	int32_t lightingType;
	float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct DirectionalLight
{
	float32_t4 color;
	float32_t3 direction;
	float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;

	float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

	float32_t4 resultColor = gMaterial.color * textureColor;

	switch (gMaterial.lightingType){
		case LIGHTING_NONE:{
				break;
			}

		case LIGHTING_LAMBERT:{
				float NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
				float cos = saturate(NdotL);

				resultColor = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
			
				break;
			}

		case LIGHTING_HALF_LAMBERT:{
				float NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
				float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

				resultColor = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;

				break;
			}

		default:{
				break;
			}
	}

	
	float resultAlpha = gMaterial.color.a * textureColor.a;

	if (resultAlpha < 0.01f)
	{
		discard;
	}

	output.color.rgb = resultColor.rgb;
	output.color.a = resultAlpha;

	return output;
}