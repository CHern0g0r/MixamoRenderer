//
// Created by chern0g0r on 05.01.2022.
//

#ifndef MIXAMORENDERER_SHADER_SOURCES_H
#define MIXAMORENDERER_SHADER_SOURCES_H

const char vertex_shader_source[] =
        R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 bone_translation[61];
uniform vec4 bone_rotation[61];
uniform float bone_scale[61];

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in ivec2 in_bone_id;
layout (location = 3) in vec2 in_bone_weight;

out vec3 normal;
out vec3 position;

vec4 quat_mult(vec4 q1, vec4 q2)
{
	return vec4(q1.x * q2.x - dot(q1.yzw, q2.yzw), q1.x * q2.yzw + q2.x * q1.yzw + cross(q1.yzw, q2.yzw));
}

vec4 quat_conj(vec4 q)
{
	return vec4(q.x, -q.yzw);
}

vec3 quat_rotate(vec4 q, vec3 v)
{
	return quat_mult(q, quat_mult(vec4(0.0, v), quat_conj(q))).yzw;
}

vec3 transform_bone(vec3 pos) {
    vec3 res = in_bone_weight.x *
        (bone_scale[in_bone_id.x] * quat_rotate(bone_rotation[in_bone_id.x], pos) + bone_translation[in_bone_id.x]) +
        in_bone_weight.y *
        (bone_scale[in_bone_id.y] * quat_rotate(bone_rotation[in_bone_id.y], pos) + bone_translation[in_bone_id.y]);
    return res;
}

vec3 transform_bone_normal(vec3 norm) {
    vec3 res = in_bone_weight.x *
           (bone_scale[in_bone_id.x] * quat_rotate(bone_rotation[in_bone_id.x], norm)) +
           in_bone_weight.y *
           (bone_scale[in_bone_id.y] * quat_rotate(bone_rotation[in_bone_id.y], norm));
    return res;
}

void main()
{
    vec3 b_pos = transform_bone(in_position);
    vec3 b_norm = transform_bone_normal(in_normal);
	gl_Position = projection * view * model * vec4(b_pos, 1.0);
	position = (model * vec4(b_pos, 1.0)).xyz;
	normal = normalize((model * vec4(b_norm, 0.0)).xyz);
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

uniform vec3 camera_position;

uniform vec3 ambient;

uniform vec3 light_direction;
uniform vec3 light_color;

in vec3 normal;
in vec3 position;

layout (location = 0) out vec4 out_color;

void main()
{
	vec3 reflected = 2.0 * normal * dot(normal, light_direction) - light_direction;
	vec3 camera_direction = normalize(camera_position - position);

	vec3 albedo = vec3(1.0, 1.0, 1.0);

	vec3 light = ambient + light_color * (max(0.0, dot(normal, light_direction)) + pow(max(0.0, dot(camera_direction, reflected)), 64.0));
	vec3 color = albedo * light;
	out_color = vec4(color, 1.0);
}
)";

const char rect_vertex_shader_source[] =
        R"(#version 330 core
layout(location = 0) in vec3 pos;

out vec2 UV;

void main(){
	gl_Position =  vec4(pos, 1);
//	UV = (gl_Position.xy+vec2(1,1))/2.0;
    UV = (gl_Position.xy + 1)/2.0;
}
)";

const char rect_fragment_shader_source[] =
        R"(#version 330 core

in vec2 UV;

out vec3 color;

uniform sampler2D renderedTexture;
uniform float time;

void main(){
    color = texture(renderedTexture, UV).xyz;
//    color = vec3(UV, 1.0);
}
)";

#endif //MIXAMORENDERER_SHADER_SOURCES_H
