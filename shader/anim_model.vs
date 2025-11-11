#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 tex;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;
layout(location = 5) in ivec4 boneIds; 
layout(location = 6) in vec4 weights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

void main()
{
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
    {
        int id = boneIds[i];
        float w = weights[i];
        if (w <= 0.0 || id < 0) continue;

        if (id >= MAX_BONES)
        {
            skinnedPos = vec4(pos, 1.0);
            skinnedNormal = norm;
            totalWeight = 1.0;
            break;
        }

        mat4 boneMat = finalBonesMatrices[id];
        skinnedPos += boneMat * vec4(pos, 1.0) * w;
        skinnedNormal += mat3(boneMat) * norm * w;
        totalWeight += w;
    }

    if (totalWeight <= 0.0)
    {
        skinnedPos = vec4(pos, 1.0);
        skinnedNormal = norm;
    }
    else if (abs(totalWeight - 1.0) > 1e-5)
    {
        skinnedPos /= totalWeight;
        skinnedNormal /= totalWeight;
    }

    gl_Position = projection * view * model * skinnedPos;

    TexCoords = tex;
    FragPos = vec3(model * skinnedPos);
    Normal = normalize(mat3(transpose(inverse(model))) * skinnedNormal);
}
