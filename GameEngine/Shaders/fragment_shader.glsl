#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;

uniform vec3 viewPos;

// Fog
uniform int   uUseFog;
uniform vec3  uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uNear;
uniform float uFar;

// Portal
uniform int   uIsPortal;
uniform float uPortalAlpha;

// ===== Material tuning =====
uniform float uAmbientStrength;   // ex: 0.25
uniform float uSpecStrength;      // ex: 0.35
uniform float uShininess;         // ex: 64.0

// ===== Light structs =====
struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;

    float cutOff;      // cos(inner)
    float outerCutOff; // cos(outer)

    float constant;
    float linear;
    float quadratic;
};

uniform int uUseDirLight;
uniform DirLight uDirLight;

uniform int uNumPointLights;
uniform PointLight uPointLights[8];

uniform int uUseSpotLight;
uniform SpotLight uSpotLight;

float linearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}

// Blinn-Phong helper
vec3 blinnPhong(vec3 N, vec3 V, vec3 L, vec3 lightColor, float intensity)
{
    float diff = max(dot(N, L), 0.0);

    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);

    vec3 ambient  = uAmbientStrength * lightColor * intensity;
    vec3 diffuse  = diff * lightColor * intensity;
    vec3 specular = uSpecStrength * spec * lightColor * intensity;

    return ambient + diffuse + specular;
}

vec3 calcDirLight(vec3 N, vec3 V)
{
    vec3 L = normalize(-uDirLight.direction);
    return blinnPhong(N, V, L, uDirLight.color, uDirLight.intensity);
}

vec3 calcPointLight(PointLight light, vec3 N, vec3 V)
{
    vec3 toLight = light.position - FragPos;
    float dist = length(toLight);
    vec3 L = toLight / max(dist, 0.0001);

    float att = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 c = blinnPhong(N, V, L, light.color, light.intensity);
    return c * att;
}

vec3 calcSpotLight(SpotLight light, vec3 N, vec3 V)
{
    vec3 toLight = light.position - FragPos;
    float dist = length(toLight);
    vec3 L = toLight / max(dist, 0.0001);

    // spotlight cone
    float theta = dot(L, normalize(-light.direction));
    float eps = max(light.cutOff - light.outerCutOff, 0.0001);
    float cone = clamp((theta - light.outerCutOff) / eps, 0.0, 1.0);

    float att = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 c = blinnPhong(N, V, L, light.color, light.intensity);
    return c * att * cone;
}

void main()
{
    vec3 albedo = texture(texture_diffuse1, TexCoords).rgb;

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    vec3 lightSum = vec3(0.0);

    if (uUseDirLight == 1)
        lightSum += calcDirLight(N, V);

    for (int i = 0; i < uNumPointLights; i++)
        lightSum += calcPointLight(uPointLights[i], N, V);

    if (uUseSpotLight == 1)
        lightSum += calcSpotLight(uSpotLight, N, V);

    vec3 lit = lightSum * albedo;

    // --- Portal glow (pãstrat din shaderul tãu) ---
    float alpha = 1.0;
    if (uIsPortal == 1)
    {
        vec2 p = TexCoords - vec2(0.5);
        float r = length(p);

        float edgeGlow = smoothstep(0.25, 0.55, r) * (1.0 - smoothstep(0.55, 0.78, r));
        float softGlow = 1.0 - smoothstep(0.0, 0.65, r);

        float glow = 0.25 * softGlow + 0.85 * edgeGlow;
        float shimmer = 0.05 * sin(20.0 * TexCoords.x) * sin(16.0 * TexCoords.y);
        glow = clamp(glow + shimmer, 0.0, 1.0);

        vec3 emissive = vec3(1.00, 0.15, 1.00) * glow;
        lit += emissive * 2.0;

        alpha = uPortalAlpha;
    }

    // --- Fog AFTER lighting ---
    if (uUseFog == 1)
    {
        float dist = linearizeDepth(gl_FragCoord.z);
        float fogFactor = clamp((uFogFar - dist) / (uFogFar - uFogNear), 0.0, 1.0);
        lit = mix(uFogColor, lit, fogFactor);
    }

    FragColor = vec4(lit, alpha);
}
