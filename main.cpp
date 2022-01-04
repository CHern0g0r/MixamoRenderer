#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

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


GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    std::uint8_t bone_ids[2];
    std::uint8_t bone_weights[2];
};

struct bone
{
    std::int32_t parent_id;
    glm::vec3 offset;
    glm::quat rotation;
};

struct bone_pose
{
    glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    float scale = 1.f;
    glm::vec3 translation = glm::vec3(0.f, 0.f, 0.f);
};

bone_pose operator * (bone_pose const & p1, bone_pose const & p2)
{
    return {p1.rotation * p2.rotation, p1.scale * p2.scale, p1.scale * glm::rotate(p1.rotation, p2.translation) + p1.translation};
}

void eval_bone_transforms(std::vector<bone_pose> & bp, std::vector<std::vector<bone_pose>> & poses,
                          std::vector<bone> & bones, int n, float t) {


    int n1 = (n+1)%6;
    bone_pose p0, p1, res;
    for (int i = 0; i<bones.size(); i++) {
        if (bones[i].parent_id == -1) {
            bp[i] = poses[n][i];
            continue;
        }
        p0 = bp[bones[i].parent_id] * poses[n][i];
        p1 = bp[bones[i].parent_id] * poses[n1][i];

        res.rotation = glm::slerp(p0.rotation, p1.rotation, 3 * t * t - 2 * t * t * t);
        res.translation = glm::mix(p0.translation, p1.translation, 3 * t * t - 2 * t * t * t);
        res.scale = glm::mix(p0.scale, p1.scale, 3 * t * t - 2 * t * t * t);
        bp[i] = res;
    }
}

void save_texture(GLuint target, const char * const filename) {

    int width, height;

    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);

    auto *img = new std::vector<char>(width * height*4);

    glGetTexImage(target, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->data());

    stbi_flip_vertically_on_write(true);

    stbi_write_png(filename, width, height, 4, img->data(), width*4);

    std::cout << "Texture wrote " << filename << '\n';

    delete img;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 10",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           800, 600,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    std::cout << width << ' ' << height << '\n';

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");

    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");

    GLuint ambient_location = glGetUniformLocation(program, "ambient");
    GLuint light_direction_location = glGetUniformLocation(program, "light_direction");
    GLuint light_color_location = glGetUniformLocation(program, "light_color");

    std::vector<GLuint> bone_rot_loc(61);
    std::vector<GLuint> bone_trans_loc(61);
    std::vector<GLuint> bone_scale_loc(61);

    for (int i = 0; i<61; i++) {
        bone_rot_loc[i] = glGetUniformLocation(program, ("bone_rotation[" + std::to_string(i) + "]").c_str());
        bone_trans_loc[i] = glGetUniformLocation(program, ("bone_translation[" + std::to_string(i) + "]").c_str());
        bone_scale_loc[i] = glGetUniformLocation(program, ("bone_scale[" + std::to_string(i) + "]").c_str());
    }

    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<bone> bones;
    std::vector<std::vector<bone_pose>> poses(6);

    {
        std::ifstream file(PRACTICE_SOURCE_DIRECTORY "/human.bin", std::ios::binary);

        std::uint32_t vertex_count;
        std::uint32_t index_count;
        file.read((char*)(&vertex_count), sizeof(vertex_count));
        file.read((char*)(&index_count), sizeof(index_count));
        vertices.resize(vertex_count);
        indices.resize(index_count);
        file.read((char*)vertices.data(), vertices.size() * sizeof(vertices[0]));
        file.read((char*)indices.data(), indices.size() * sizeof(indices[0]));
    }

    {
        std::ifstream file(PRACTICE_SOURCE_DIRECTORY "/bones.bin", std::ios::binary);

        std::uint32_t bone_count;
        file.read((char*)(&bone_count), sizeof(bone_count));
        bones.resize(bone_count);
        file.read((char*)(bones.data()), bones.size() * sizeof(bones[0]));
    }

    for (std::size_t i = 0; i < 6; ++i)
    {
        std::ifstream file(PRACTICE_SOURCE_DIRECTORY "/pose_" + std::to_string(i) + ".bin", std::ios::binary);

        poses[i].resize(bones.size());
        file.read((char*)(poses[i].data()), poses[i].size() * sizeof(poses[i][0]));
    }

    std::cout << "Loaded " << vertices.size() << " vertices, " << indices.size() << " indices, " << bones.size() << " bones" << std::endl;

    std::vector<bone_pose> bone_transforms(61);

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(12));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 2, GL_UNSIGNED_BYTE, sizeof(vertex), (void*)(24));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)(26));


//    ------------------------------------------

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    GLuint renderedTexture;
    glGenTextures(1, &renderedTexture);
    glBindTexture(GL_TEXTURE_2D, renderedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0);

    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);

    GLuint depthrenderbuffer;
    glGenRenderbuffers(1, &depthrenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;

//    -------------------------------------------

    GLuint quad_VertexArrayID;
    glGenVertexArrays(1, &quad_VertexArrayID);
    glBindVertexArray(quad_VertexArrayID);

    static const GLfloat g_quad_vertex_buffer_data[] = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f,  1.0f, 0.0f,
    };

    GLuint rect_vao;
    glGenVertexArrays(1, &rect_vao);
    glBindVertexArray(rect_vao);

    GLuint quad_vertexbuffer;
    glGenBuffers(1, &quad_vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*) 0);

    auto rect_vertex_shader = create_shader(GL_VERTEX_SHADER, rect_vertex_shader_source);
    auto rect_fragment_shader = create_shader(GL_FRAGMENT_SHADER, rect_fragment_shader_source);
    auto rect_program = create_program(rect_vertex_shader, rect_fragment_shader);
    GLuint texID = glGetUniformLocation(rect_program, "renderedTexture");
    GLuint timeID = glGetUniformLocation(rect_program, "time");


    static_assert(sizeof(vertex) == 28);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    float view_angle = 0.f;
    float camera_distance = 3.f;
    float camera_height = 1.2f;

    float model_rotation = 0.f;

    bool save = false;

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT: switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;

                            glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
                            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

                            glBindTexture(GL_TEXTURE_2D, renderedTexture);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        if (button_down[SDLK_UP])
            camera_distance -= 3.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 3.f * dt;

        if (button_down[SDLK_LEFT])
            model_rotation -= 3.f * dt;
        if (button_down[SDLK_RIGHT])
            model_rotation += 3.f * dt;

        if (button_down[SDLK_p]){
            save = true;
        }

        glClearColor(0.8f, 0.8f, 1.f, 0.f);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glUseProgram(program);

        float near = 0.1f;
        float far = 100.f;

        glm::mat4 model(1.f);
        model = glm::rotate(model, model_rotation, {0.f, 1.f, 0.f});
        model = glm::rotate(model, -glm::pi<float>() / 2.f, {1.f, 0.f, 0.f});

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, -camera_height, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        int numpose = (int) floor(time) % 6;
        float ts = time - floor(time);

        eval_bone_transforms(bone_transforms, poses, bones, numpose, ts);

        for (int i = 0; i<bone_transforms.size(); i++) {
            glUniform1f(bone_scale_loc[i], bone_transforms[i].scale);
            glUniform3f(bone_trans_loc[i], bone_transforms[i].translation.x, bone_transforms[i].translation.y, bone_transforms[i].translation.z);
            glUniform4f(bone_rot_loc[i], bone_transforms[i].rotation.w, bone_transforms[i].rotation.x, bone_transforms[i].rotation.y, bone_transforms[i].rotation.z);
        }

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));

        glUniform3fv(camera_position_location, 1, (float*)(&camera_position));

        glUniform3f(ambient_location, 0.2f, 0.2f, 0.4f);
        glUniform3f(light_direction_location, 1.f / std::sqrt(3.f), 1.f / std::sqrt(3.f), 1.f / std::sqrt(3.f));
        glUniform3f(light_color_location, 0.8f, 0.3f, 0.f);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        glClear( GL_DEPTH_BUFFER_BIT);
        glUseProgram(rect_program);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderedTexture);
        glUniform1i(texID, 0);

        glUniform1f(timeID, (float)(time) );

        glBindVertexArray(rect_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (save) {
            save_texture(GL_TEXTURE_2D, "pict.png");
            save = false;
        }

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
