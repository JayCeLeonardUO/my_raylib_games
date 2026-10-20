#version 100

// Three-point lighting, instanced, GLSL ES 1.00 (WebGL 1 + ANGLE_instanced_arrays).

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
attribute mat4 instanceTransform;

uniform mat4 mvp;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;
varying vec4 fragTangent;

void main()
{
    vec4 world = instanceTransform * vec4(vertexPosition, 1.0);
    fragPosition = world.xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(mat3(instanceTransform) * vertexNormal);
    fragTangent = vec4(0.0, 0.0, 0.0, 1.0);
    gl_Position = mvp * world;
}
