#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;

uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

// ===== Fog uniforms =====
uniform int   uUseFog;     // 0/1
uniform vec3  uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uNear;       // camera near (0.1)
uniform float uFar;        // camera far  (10000)

// Convert depth buffer value to linear distance
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}

void main()
{
    vec3 albedo = texture(texture_diffuse1, TexCoords).rgb;

    // basic lighting (same idea as before)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 ambient = 0.25 * lightColor;

    vec3 color = (ambient + diffuse) * albedo;

    // ===== Underwater fog =====
    if (uUseFog == 1)
    {
        float linearDepth = LinearizeDepth(gl_FragCoord.z);

        // fogFactor = 1 near, 0 far
        float fogFactor = clamp((uFogFar - linearDepth) / (uFogFar - uFogNear), 0.0, 1.0);

        // mix fog -> color
        color = mix(uFogColor, color, fogFactor);
    }

    FragColor = vec4(color, 1.0);
}
