#version 330

// Three-point lighting: vertex stage. Passes world-space position and normal to the
// fragment shader. Matches raylib's default attribute/uniform names so DrawMesh works.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexTangent;   // xyz tangent, w handedness (GenMeshTangents); zeros if the mesh has none

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragTangent;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragTangent = vec4(vec3(matModel * vec4(vertexTangent.xyz, 0.0)), vertexTangent.w);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
