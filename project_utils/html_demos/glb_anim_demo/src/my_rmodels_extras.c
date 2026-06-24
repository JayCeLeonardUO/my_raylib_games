#include "my_rmodels_extras.h"
#include <raymath.h>
#include <rlgl.h>
#include <stddef.h>

void UpdateModelAnimationWithScale(Model model, ModelAnimation anim, int frame)
{
    if (anim.frameCount <= 0 || anim.bones == NULL || anim.framePoses == NULL) return;
    if (frame >= anim.frameCount) frame = frame % anim.frameCount;

    // --- Phase 1: Compute bone matrices with correct SRT order ---
    for (int m = 0; m < model.meshCount; m++)
    {
        if (model.meshes[m].boneMatrices == NULL) continue;

        for (int boneId = 0; boneId < model.meshes[m].boneCount; boneId++)
        {
            // Bind pose (rest pose)
            Vector3 inTranslation = model.bindPose[boneId].translation;
            Quaternion inRotation = model.bindPose[boneId].rotation;
            Vector3 inScale = model.bindPose[boneId].scale;

            // Animated pose for this frame
            Vector3 outTranslation = anim.framePoses[frame][boneId].translation;
            Quaternion outRotation = anim.framePoses[frame][boneId].rotation;
            Vector3 outScale = anim.framePoses[frame][boneId].scale;

            // Inverse bind pose
            Quaternion invRotation = QuaternionInvert(inRotation);
            Vector3 invTranslation = Vector3RotateByQuaternion(
                Vector3Negate(inTranslation), invRotation);
            Vector3 invScale = (Vector3){
                (inScale.x != 0.0f) ? 1.0f / inScale.x : 1.0f,
                (inScale.y != 0.0f) ? 1.0f / inScale.y : 1.0f,
                (inScale.z != 0.0f) ? 1.0f / inScale.z : 1.0f,
            };

            // Relative transform = outPose * inv(bindPose)
            Quaternion boneRotation = QuaternionMultiply(outRotation, invRotation);
            Vector3 boneScale = Vector3Multiply(outScale, invScale);
            Vector3 boneTranslation = Vector3Add(
                Vector3RotateByQuaternion(
                    Vector3Multiply(invTranslation, outScale), outRotation),
                outTranslation);

            // Build bone matrix with proper SRT order:
            // M = T * R * S  (applied to vertex: first scale, then rotate, then translate)
            Matrix matS = MatrixScale(boneScale.x, boneScale.y, boneScale.z);
            Matrix matR = QuaternionToMatrix(boneRotation);
            Matrix matT = MatrixTranslate(boneTranslation.x, boneTranslation.y, boneTranslation.z);

            // M = S then R then T (raylib's MatrixMultiply applies left first)
            model.meshes[m].boneMatrices[boneId] = MatrixMultiply(
                matS, MatrixMultiply(matR, matT));
        }
    }

    // --- Phase 2: Vertex skinning with explicit scale application ---
    for (int m = 0; m < model.meshCount; m++)
    {
        Mesh mesh = model.meshes[m];
        if (mesh.animVertices == NULL || mesh.boneIds == NULL || mesh.boneWeights == NULL) continue;

        int boneCounter = 0;
        int vValues = mesh.vertexCount * 3;
        bool updated = false;

        for (int vCounter = 0; vCounter < vValues; vCounter += 3)
        {
            mesh.animVertices[vCounter]     = 0;
            mesh.animVertices[vCounter + 1] = 0;
            mesh.animVertices[vCounter + 2] = 0;

            if (mesh.animNormals != NULL)
            {
                mesh.animNormals[vCounter]     = 0;
                mesh.animNormals[vCounter + 1] = 0;
                mesh.animNormals[vCounter + 2] = 0;
            }

            for (int j = 0; j < 4; j++, boneCounter++)
            {
                float boneWeight = mesh.boneWeights[boneCounter];
                int boneId = mesh.boneIds[boneCounter];

                if (boneWeight == 0.0f) continue;

                // Transform vertex by bone matrix (SRT already baked in)
                Vector3 inVertex = (Vector3){
                    mesh.vertices[vCounter],
                    mesh.vertices[vCounter + 1],
                    mesh.vertices[vCounter + 2]
                };
                Vector3 animVertex = Vector3Transform(inVertex,
                    model.meshes[m].boneMatrices[boneId]);

                mesh.animVertices[vCounter]     += animVertex.x * boneWeight;
                mesh.animVertices[vCounter + 1] += animVertex.y * boneWeight;
                mesh.animVertices[vCounter + 2] += animVertex.z * boneWeight;
                updated = true;

                // Normals
                if (mesh.normals != NULL && mesh.animNormals != NULL)
                {
                    Vector3 inNormal = (Vector3){
                        mesh.normals[vCounter],
                        mesh.normals[vCounter + 1],
                        mesh.normals[vCounter + 2]
                    };
                    Vector3 animNormal = Vector3Transform(inNormal,
                        model.meshes[m].boneMatrices[boneId]);
                    // Don't scale normals — just transform by rotation part
                    mesh.animNormals[vCounter]     += animNormal.x * boneWeight;
                    mesh.animNormals[vCounter + 1] += animNormal.y * boneWeight;
                    mesh.animNormals[vCounter + 2] += animNormal.z * boneWeight;
                }
            }
        }

        if (updated)
        {
            rlUpdateVertexBuffer(mesh.vboId[0], mesh.animVertices,
                mesh.vertexCount * 3 * sizeof(float), 0);
            if (mesh.animNormals != NULL) {
                rlUpdateVertexBuffer(mesh.vboId[2], mesh.animNormals,
                    mesh.vertexCount * 3 * sizeof(float), 0);
            }
        }
    }
}
