#version 100

// Three-point lighting, vertex stage, GLSL ES 1.00 for the web (WebGL 1) build.
// Same uniforms/attributes as three_point.vs so the C++ side is unchanged.

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
attribute vec4 vertexTangent;   // xyz tangent, w handedness (GenMeshTangents); zeros if the mesh has none

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;
varying vec4 fragTangent;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragTangent = vec4(vec3(matModel * vec4(vertexTangent.xyz, 0.0)), vertexTangent.w);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
