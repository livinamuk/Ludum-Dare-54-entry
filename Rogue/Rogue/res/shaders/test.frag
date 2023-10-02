#version 420 core

//layout (binding = 0) uniform sampler2D tex;
layout (binding = 0) uniform sampler3D tex;
layout (binding = 1) uniform samplerCube shadowMap0;
layout (binding = 2) uniform samplerCube shadowMap1;
layout (binding = 3) uniform samplerCube shadowMap2;
layout (binding = 4) uniform samplerCube shadowMap3;
layout (binding = 5) uniform sampler2D basecolorTexture;
layout (binding = 6) uniform sampler2D plasterTexture;

uniform vec3 lightPosition[4];
uniform vec3 lightColor[4];
uniform float lightStrength[4];
uniform float lightRadius[4];

in vec3 Normal;
in vec2 TexCoord;
in vec3 WorldPos;

in vec3 backupNormal;

out vec4 FragColor;
uniform vec3 viewPos;
uniform vec3 camForward;
uniform vec3 lightingColor;
uniform vec3 indirectLightingColor;
uniform vec3 baseColor;
uniform int mode;
uniform int materialID;

uniform mat4 view;

#define MAP_WIDTH   32
#define MAP_HEIGHT  22
#define MAP_DEPTH   36
#define VOXEL_SIZE  0.2
#define PROPOGATION_SPACING 1.0
#define PROBE_SPACING  VOXEL_SIZE * PROPOGATION_SPACING

uniform int tex_flag;

uniform bool hurt;
uniform float stabAmount;

vec3 filmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

float filmic(float x) {
  float X = max(0.0, x - 0.004);
  float result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, 2.2);
}

float calculateDoomFactor(vec3 fragPos, vec3 camPos, float beginDistance, float scale)
{
    float distanceFromCamera = distance(fragPos, camPos);
    float doomFactor = 1;	
    if (distanceFromCamera> beginDistance) {
        distanceFromCamera -= beginDistance;
        doomFactor = (1 - distanceFromCamera);
        doomFactor *= scale;
        doomFactor = clamp(doomFactor, 0.1, 1.0);
    }
    return doomFactor;
}

float getFogFactor(vec3 fragPos, vec3 camPos) {
    float d = distance(fragPos, camPos);
    float b = 0.05;
    return 1.0 - exp( -d * b );
}

float inverseFalloff(float x){
    // 10 is a good number, but you can also try "iMouse.y" to test values
    float const1 = 10.0;
    float xSq = x*x;
    return (1.0-xSq)/(const1*xSq+1.0);
}

vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);
  

float ShadowCalculation(samplerCube depthTex, vec3 lightPos, vec3 fragPos, vec3 viewPos)
{
   // fragPos += Normal * 0.5;
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    float far_plane = 20.0;

    float shadow = 0.0;
    //float bias = 0.0075;
    vec3 lightDir = fragPos - lightPos;
    float bias = max(0.0125 * (1.0 - dot(Normal, lightDir)), 0.00125);
    int samples = 20;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 2000.0;
    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(depthTex, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= far_plane;   // undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return 1 - shadow;
}

vec3 GetDirectLighting(vec3 baseColor, vec3 lightPos, vec3 lightColor, float radius, float strength) {
    float dist = distance(WorldPos, lightPos);
    //float att = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist);
    float att = smoothstep(radius, 0.0, length(lightPos - WorldPos));
    vec3 n = normalize(Normal);
    vec3 l = normalize(lightPos - WorldPos);
    float ndotl = clamp(dot(n, l), 0.0001f, 0.9999f);

    //return vec3(lightColor) * att * strength * ndotl * 2;


    float specualAmount = 0;

        // skin
        if (materialID == 1) {
            specualAmount = 4;
        
        }        
        // metal
        if (materialID == 2) {
            specualAmount = 4;
        }   
        // wood
        if (materialID == 3) {
            specualAmount = 1;
        }
        // knife blade
        if (materialID == 4) {
            specualAmount = 4;
        }


    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 diffuse = lightColor * ndotl * baseColor * 3;  
        

    vec3 lightDir = l;
    
    // specular
    //vec3 reflectDir = reflect(-lightDir, norm);

    // specular
    /*vec3 viewDir = normalize(viewPos - WorldPos);
    //vec3 reflectDir = reflect(-(lightPos - WorldPos), norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = vec3(spec);  
        
    // apply attenuation
    // diffuse *= att;
    //specular *= att;   
    */

    vec3 viewDir = normalize(viewPos - WorldPos);
    vec3 reflectDir = reflect(-l, norm);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(norm, halfwayDir), 0.0), 64);
    vec3 specular = spec * lightColor * specualAmount; 


    // ambient
    vec3 ambient = vec3(0);
            
    vec3 result = (ambient + diffuse + specular) *  att * ndotl * strength;
  //  vec3 result = (ambient + diffuse ) *  att * ndotl * min(strength, 1.5);
   // result = specular;
    // result.g=0;
    // result.b = 0;
    return result;

}

void main()
{
    float doom = calculateDoomFactor(WorldPos, viewPos, 4, 1);
    doom = getFogFactor(WorldPos, viewPos);
    
    // uniform color
    FragColor = vec4(Normal * 0.5 + 0.5, 1.0f);
    FragColor = vec4(baseColor * lightingColor, 1.0f);

    float worldSpaceMapWidth = MAP_WIDTH * VOXEL_SIZE;
    float worldSpaceMapHeight = MAP_HEIGHT * VOXEL_SIZE;
    float worldSpaceMapDepth = MAP_DEPTH * VOXEL_SIZE;

  
    
    // Basecolor
    vec3 baseColor =  texture2D(basecolorTexture, TexCoord).rgb;

    // Material special cases
    {
        // arms
        if (materialID == 1) {
            baseColor.x = 250.0 / 255.0;
            baseColor.y = 185.0 / 255.0;
            baseColor.z = 129.0 / 255.0;
        }        
        // metal
        if (materialID == 2) {
            baseColor = vec3(0.3);
        }   
        // wood
        if (materialID == 3) {
            baseColor.x = 183.0 / 255.0;
            baseColor.y = 71.0 / 255.0;
            baseColor.z = 4.0 / 255.0;
        }
        // knife blade
        if (materialID == 4) {
            baseColor = vec3(0.99);
        }
    }

    

   /*if (tex_flag == 1 ) {
        composite *= baseColor;
    }
    if (tex_flag == 2 && WorldPos.y < 2.5) {
        composite *= baseColor;
    }
    if (tex_flag == 3 && WorldPos.y < 2.5) {
        composite *= baseColor;
    }
    */
    
    if (tex_flag > 0 && WorldPos.y > 2.50001) {
        baseColor = texture2D(plasterTexture, TexCoord).rgb;
    }
    

    // Direct lighting
    float shadow0 = ShadowCalculation(shadowMap0, lightPosition[0], WorldPos, viewPos);
    vec3 ligthting0 = GetDirectLighting(baseColor, lightPosition[0], lightColor[0], lightRadius[0], lightStrength[0]);
    float shadow1 = ShadowCalculation(shadowMap1, lightPosition[1], WorldPos, viewPos);
    vec3 ligthting1 = GetDirectLighting(baseColor, lightPosition[1], lightColor[1], lightRadius[1], lightStrength[1]);
    float shadow2 = ShadowCalculation(shadowMap2, lightPosition[2], WorldPos, viewPos);
    vec3 ligthting2 = GetDirectLighting(baseColor, lightPosition[2], lightColor[2], lightRadius[2], lightStrength[2]);
    float shadow3 = ShadowCalculation(shadowMap3, lightPosition[3], WorldPos, viewPos);
    vec3 ligthting3 = GetDirectLighting(baseColor, lightPosition[3], lightColor[3], lightRadius[3], lightStrength[3]);
     
    vec3 directLighting = vec3(0);
    directLighting += shadow0 * ligthting0;
   /// directLighting += shadow1 * ligthting1;
    directLighting += shadow2 * ligthting2;
    directLighting += shadow3 * ligthting3;
    
    // Composite
    vec3 composite = directLighting;



    composite = filmic(composite);
    composite.r -= 0.00125;



   //composite = baseColor * directLighting;


   // composite = Normal;

    
     if (tex_flag == 0) {
  //      composite = vec3(1,0,0);
    }

    // Fog
    vec3 fogColor = vec3(0.050);
    composite = mix(composite, fogColor, doom);
    
    // Final color
    if (mode == 0) {
        FragColor.rgb = composite;
    }
    if (mode == 1) {
        FragColor.rgb = lightingColor;
    }
    else if (mode == 2) {
        FragColor.rgb = directLighting;
    }
    
    
    
  //  FragColor.rgb = Normal;

    /*vec3 lightDir2 = (inverse(view) * vec4(0, 0, 1, 0)).xyz;
    lightDir2 = normalize(vec3(0, 1, 0));
    float ndotL = dot(Normal, lightDir2);
     ndotL = dot(lightDir2, Normal);
    FragColor.rgb = vec3(ndotL);*/

     if (materialID == 4)
     {
      //FragColor.rgb = vec3(1,0,0);
     }
   //   FragColor.rgb = normalize(Normal) * 0.2;

   if (hurt) {
   // FragColor.rgb *= vec3(1,0,0);
    FragColor.g *= stabAmount ;
    FragColor.b *= stabAmount;
    FragColor.g -= 0.023;
    FragColor.b -= 0.023;
    FragColor.rgb += vec3(0.015);
  //  FragColor.rgb *= vec3(1,0,0);
   }

}
