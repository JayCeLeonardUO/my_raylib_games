#version 330

// Three-point lighting, instanced: the model matrix comes per instance as a vertex
// attribute (raylib DrawMeshInstanced). Same outputs as three_point.vs, same fragment shader.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in mat4 instanceTransform;

uniform mat4 mvp; // view * projection; raylib leaves the model part to instanceTransform

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragTangent;

void main()
{
    vec4 world = instanceTransform * vec4(vertexPosition, 1.0);
    fragPosition = world.xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    // rotation + uniform scale only, so the model matrix doubles as the normal matrix
    fragNormal = normalize(mat3(instanceTransform) * vertexNormal);
    fragTangent = vec4(0.0, 0.0, 0.0, 1.0);
    gl_Position = mvp * world;
}
