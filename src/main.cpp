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

#include "shader_sources.h"
#include "shader.h"
#include "utils.h"
#include "errors.h"
#include "model.h"

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

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

    Model m;
    std::string filepath("/home/chern0g0r/workspace/diploma/models/Taunt.dae");
    m.load(filepath);

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
