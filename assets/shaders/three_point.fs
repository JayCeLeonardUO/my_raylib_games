#version 330

// Three-point lighting: key + fill + rim, all directional and camera-relative.
// Directions are "toward the light", in world space, normalized on the CPU.
// Colors arrive pre-multiplied by their per-light intensity.

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragTangent;

uniform sampler2D texture0;
uniform sampler2D texture2;   // tangent-space normal map (raylib binds MATERIAL_MAP_NORMAL here)
uniform int useNormalMap;     // 1 when the material has a normal map texture
uniform vec4 colDiffuse;

uniform vec3 viewPos;
uniform vec3 keyDir;
uniform vec3 keyColor;
uniform vec3 fillDir;
uniform vec3 fillColor;
uniform vec3 rimDir;
uniform vec3 rimColor;
uniform vec3 ambientColor;
uniform float intensity;     // master multiplier
uniform float specStrength;  // key-light Blinn-Phong highlight
uniform float specPower;
uniform float rimPower;      // how tight the rim hugs the silhouette

out vec4 finalColor;

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N; // double-sided: thin particles are lit from both faces
    if (useNormalMap == 1) {
        // perturb the geometric normal with the normal map, in the vertex tangent frame
        vec3 T = normalize(fragTangent.xyz - N * dot(N, fragTangent.xyz));
        vec3 B = cross(N, T) * fragTangent.w;
        vec3 nm = texture(texture2, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(T * nm.x + B * nm.y + N * nm.z);
    }
    vec3 V = normalize(viewPos - fragPosition);

    // key: plain Lambert, this is the light that defines form
    float kd = max(dot(N, keyDir), 0.0);

    // fill: half-Lambert so it wraps softly into the key's shadow side
    float fd = dot(N, fillDir) * 0.5 + 0.5;
    fd = fd * fd;

    // rim: light from behind only shows on grazing silhouettes facing it
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), rimPower);
    float rd = max(dot(N, rimDir), 0.0);
    float rim = fresnel * rd;

    // key specular (Blinn-Phong), suppressed where the key does not reach
    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), specPower) * specStrength * step(0.001, kd);

    vec3 light = ambientColor + keyColor * kd + fillColor * fd + rimColor * rim;
    vec3 color = (albedo.rgb * light + keyColor * spec) * intensity;

    finalColor = vec4(color, albedo.a);
}
