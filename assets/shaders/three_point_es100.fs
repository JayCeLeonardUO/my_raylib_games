#version 100
precision mediump float;

// Three-point lighting, fragment stage, GLSL ES 1.00 for the web (WebGL 1) build.
// Mirrors three_point.fs: key + fill + rim, directional and camera-relative.

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;
varying vec4 fragTangent;

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
uniform float intensity;
uniform float specStrength;
uniform float specPower;
uniform float rimPower;

void main()
{
    vec4 albedo = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;

    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N; // double-sided: thin particles are lit from both faces
    if (useNormalMap == 1) {
        // perturb the geometric normal with the normal map, in the vertex tangent frame
        vec3 T = normalize(fragTangent.xyz - N * dot(N, fragTangent.xyz));
        vec3 B = cross(N, T) * fragTangent.w;
        vec3 nm = texture2D(texture2, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(T * nm.x + B * nm.y + N * nm.z);
    }
    vec3 V = normalize(viewPos - fragPosition);

    float kd = max(dot(N, keyDir), 0.0);

    float fd = dot(N, fillDir) * 0.5 + 0.5;
    fd = fd * fd;

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), rimPower);
    float rd = max(dot(N, rimDir), 0.0);
    float rim = fresnel * rd;

    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), specPower) * specStrength * step(0.001, kd);

    vec3 light = ambientColor + keyColor * kd + fillColor * fd + rimColor * rim;
    vec3 color = (albedo.rgb * light + keyColor * spec) * intensity;

    gl_FragColor = vec4(color, albedo.a);
}
