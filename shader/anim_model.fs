#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform float alphaCutoff; // discard fragments below this

void main()
{
    vec4 tex = texture(texture_diffuse1, TexCoords);

    if (tex.a < alphaCutoff)
        discard;

    FragColor = tex;
}
