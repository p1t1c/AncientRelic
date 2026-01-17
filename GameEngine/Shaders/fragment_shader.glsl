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

// Portal
uniform int   uIsPortal;       // 0 normal, 1 portal
uniform float uPortalAlpha;    // ex: 0.25 - 0.45

float linearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}

void main()
{
    vec3 albedo = texture(texture_diffuse1, TexCoords).rgb;

    // --- Lighting ---
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);

    float diff = max(dot(N, L), 0.0);

    float ambientStrength = 0.70;
    float diffuseStrength = 1.10;
    float specStrength    = 0.12;

    vec3 ambient  = ambientStrength * lightColor;
    vec3 diffuse  = diffuseStrength * diff * lightColor;

    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 32.0);
    vec3 specular = specStrength * spec * lightColor;

    vec3 lit = (ambient + diffuse + specular) * albedo;

    // --- Portal glow (subtle emissive + transparency) ---
    float alpha = 1.0;

    if (uIsPortal == 1)
    {
        // Glow stronger at edges based on UV distance from center
        vec2 p = TexCoords - vec2(0.5);
        float r = length(p);

        float edgeGlow = smoothstep(0.25, 0.55, r) * (1.0 - smoothstep(0.55, 0.78, r));
        float softGlow = 1.0 - smoothstep(0.0, 0.65, r);

        float glow = 0.25 * softGlow + 0.85 * edgeGlow;

        // a bit of animated shimmer (optional, low amplitude)
        // (if you don't want animation, delete next 2 lines and keep glow)
        float shimmer = 0.05 * sin(20.0 * TexCoords.x) * sin(16.0 * TexCoords.y);
        glow = clamp(glow + shimmer, 0.0, 1.0);

        vec3 emissive = vec3(1.00, 0.15, 1.00) * glow;  // magenta neon


        lit += emissive*2;

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
