#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 aBoneID;
layout (location = 6) in vec4 aBoneWeight;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 Normal;
out vec2 TexCoord;
out vec3 WorldPos;


uniform bool isAnimated;
//out vec3 attrNormal;
//out vec3 attrTangent;
//out vec3 attrBiTangent;
uniform mat4 skinningMats[128];

uniform int tex_flag;

void main() {

	// ANIMATED!!!!!!!!!
	
	if (isAnimated) {

		TexCoord = aTexCoord;	
		//vec4 worldPos;
		vec4 totalLocalPos = vec4(0.0);
		vec4 totalNormal = vec4(0.0);
			
		vec4 vertexPosition =  vec4(aPos, 1.0);
		vec4 vertexNormal = vec4(aNormal, 0.0);

		for(int i=0;i<4;i++)  {
			mat4 jointTransform = skinningMats[int(aBoneID[i])];
			vec4 posePosition =  jointTransform  * vertexPosition * aBoneWeight[i];
			
			vec4 worldNormal = jointTransform * vertexNormal * aBoneWeight[i];

			totalLocalPos += posePosition;		
			totalNormal += worldNormal;
		}
	
		WorldPos = totalLocalPos.xyz;
		//WorldPos = aPos;
		//WorldPos.y += 1;

		Normal = normalize(totalNormal.xyz);

		gl_Position = projection * view * vec4(WorldPos, 1.0);

		//attrNormal = (model * vec4(Normal, 0.0)).xyz;
		//attrTangent = (model * vec4(aTangent, 0.0)).xyz;
		//attrBiTangent = (model * vec4(aBitangent, 0.0)).xyz;
	}


	// NOT ANIMATED

	else {
	
		Normal = (model * vec4(aNormal, 0)).xyz;
		TexCoord = aTexCoord;
		TexCoord.y = -TexCoord.y;
		WorldPos = (model * vec4(aPos.x, aPos.y, aPos.z, 1.0)).xyz;

		gl_Position = projection * view * vec4(WorldPos, 1.0);
	
		if (tex_flag == 1)
			TexCoord = vec2(WorldPos.z, WorldPos.x)  * 1.0;

		WorldPos.y -= 0.1;
		if (tex_flag == 2)
			TexCoord = vec2(WorldPos.z / 2.4, WorldPos.y / 2.4) ;
		if (tex_flag == 3)
			TexCoord = vec2(WorldPos.x / 2.4, WorldPos.y / 2.4) ;
		WorldPos.y += 0.1;

		
	//	Normal = vec3(0,1,1);

	}

	
}