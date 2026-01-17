#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;

uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

// Fog
uniform int   uUseFog;
uniform vec3  uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uNear;
uniform float uFar;

float linearizeDepth(float depth)
{
    // depth in [0..1] from gl_FragCoord.z
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}

void main()
{
    vec3 albedo = texture(texture_diffuse1, TexCoords).rgb;

    // --- Lighting (strong ambient so textures are readable) ---
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);

    float diff = max(dot(N, L), 0.0);

    // ambient/diffuse/spec tuning (make textures pop)
    float ambientStrength = 0.70;    // BIG ambient -> visible
    float diffuseStrength = 1.10;
    float specStrength    = 0.12;

    vec3 ambient = ambientStrength * lightColor;

    vec3 diffuse = diffuseStrength * diff * lightColor;

    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 32.0);
    vec3 specular = specStrength * spec * lightColor;

    vec3 lit = (ambient + diffuse + specular) * albedo;

    // --- Fog (apply AFTER lighting, so it doesn't kill texture) ---
    if (uUseFog == 1)
    {
        float dist = linearizeDepth(gl_FragCoord.z);

        float fogFactor = clamp((uFogFar - dist) / (uFogFar - uFogNear), 0.0, 1.0);
        // fogFactor=1 -> no fog, fogFactor=0 -> full fog
        lit = mix(uFogColor, lit, fogFactor);
    }

    FragColor = vec4(lit, 1.0);
}
