
#include "cubescape.h"

#include <glbinding/gl/gl.h>

#include "glutils.h"
#include "rawfile.h"


using namespace gl;

CubeScape::CubeScape()
: a_vertex(-1)
, u_transform(-1)
, u_time(-1)
, m_vao(0)
, m_indices(0)
, m_vertices(0)
, m_program(0)
, m_a(0.f)
{
    static const char * vertSource = R"(
        #version 150 core
        
        in vec3 a_vertex;
        out float v_h;

        uniform sampler2D terrain;
        uniform float time;

        void main()
        {
            vec2 uv = vec2(mod(gl_InstanceID, 32), int(gl_InstanceID / 32)) * 0.0625;

            vec3 v = a_vertex * 0.0625;
            v.xz += uv * 2.0 - vec2(2.0);

            v_h = texture2D(terrain, uv * 0.6 + vec2(sin(time * 0.04), time * 0.02)).r;
            v.y += v_h;

            gl_Position = vec4(v, 1.0); 
        })";

    static const char * geomSource = R"(
        #version 150 core

        uniform mat4 modelViewProjection;

        in  float v_h[3];
        out float g_h;

        out vec2 g_uv;
        out vec3 g_normal;

        layout (triangles) in;
        layout (triangle_strip, max_vertices = 4) out;

        void main()
        {
            vec4 u = gl_in[1].gl_Position - gl_in[0].gl_Position;
            vec4 v = gl_in[2].gl_Position - gl_in[0].gl_Position;

            vec3 n = cross(normalize((modelViewProjection * u).xyz), normalize((modelViewProjection * v).xyz));

            gl_Position = modelViewProjection * gl_in[0].gl_Position;
            g_uv = vec2(0.0, 0.0);
            g_normal = n;
            g_h = v_h[0];
            EmitVertex();

            gl_Position = modelViewProjection * gl_in[1].gl_Position;
            g_uv = vec2(1.0, 0.0);
            EmitVertex();

            gl_Position = modelViewProjection * gl_in[2].gl_Position;
            g_uv = vec2(0.0, 1.0);
            EmitVertex();

            gl_Position = modelViewProjection * vec4((gl_in[0].gl_Position + u + v).xyz, 1.0);
            g_uv = vec2(1.0, 1.0);
            EmitVertex();
        })";

    static const char * fragSource = R"(
        #version 150 core

        in float g_h;
        
        in vec2 g_uv;
        in vec3 g_normal;

        out vec4 fragColor;

        uniform sampler2D patches;

        void main()
        {
            vec3 n = normalize(g_normal);
            vec3 l = normalize(vec3(0.0, -0.5, 1.0));

            float lambert = dot(n, l);

            float t = (1.0 - g_h) * 4.0 - 1.0;
            vec2 uv = g_uv * vec2(0.25, 1.0);

            vec4 c0 = texture2D(patches, uv + max(floor(t), 0.0) * vec2(0.25, 0.0));
            vec4 c1 = texture2D(patches, uv + min(floor(t) + 1.0, 3.0) * vec2(0.25, 0.0));

            fragColor = mix(c0, c1, smoothstep(0.25, 0.75, fract(t))) * lambert;
        })";

    // create program

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vs, 1, &vertSource, nullptr);
    glCompileShader(vs);
    compile_info(vs);

    glShaderSource(gs, 1, &geomSource, nullptr);
    glCompileShader(gs);
    compile_info(gs);

    glShaderSource(fs, 1, &fragSource, nullptr);
    glCompileShader(fs);
    compile_info(fs);

    m_program = glCreateProgram();

    glAttachShader(m_program, vs);
    glAttachShader(m_program, gs);
    glAttachShader(m_program, fs);

    glLinkProgram(m_program);

    // create textures

    glGenTextures(2, m_textures);

    glBindTexture(GL_TEXTURE_2D, m_textures[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(GL_REPEAT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(GL_REPEAT));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<int>(GL_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<int>(GL_LINEAR));

    {
        RawFile terrain("data/cubescape/terrain.64.64.r.ub.raw");
        if (!terrain.isValid())
            std::cout << "warning: loading texture from " << terrain.filePath() << " failed.";

        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(GL_LUMINANCE8), 64, 64, 0, GL_RED, GL_UNSIGNED_BYTE, terrain.data());
    }

    glBindTexture(GL_TEXTURE_2D, m_textures[1]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(GL_REPEAT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(GL_REPEAT));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<int>(GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<int>(GL_NEAREST));

    {
        RawFile patches("data/cubescape/patches.64.16.rgb.ub.raw");
        if (!patches.isValid())
            std::cout << "warning: loading texture from " << patches.filePath() << " failed.";

        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(GL_RGB8), 64, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, patches.data());
    }


    // create cube

    static const GLfloat vertices_data[24] =
    {
        -1.f, -1.f, -1.f, // 0
        -1.f, -1.f,  1.f, // 1
        -1.f,  1.f, -1.f, // 2
        -1.f,  1.f,  1.f, // 3
         1.f, -1.f, -1.f, // 4
         1.f, -1.f,  1.f, // 5
         1.f,  1.f, -1.f, // 6
         1.f,  1.f,  1.f  // 7
    };

    static const GLubyte indices_data[18] = {
        2, 3, 6, 0, 1, 2, 1, 5, 3, 5, 4, 7, 4, 0, 6, 5, 1, 4 };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    glBufferData(GL_ARRAY_BUFFER, (8 * 3) * sizeof(float), vertices_data, GL_STATIC_DRAW);

    glGenBuffers(1, &m_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (6 * 3) * sizeof(GLubyte), indices_data, GL_STATIC_DRAW);

    // setup uniforms

    a_vertex = glGetAttribLocation(m_program, "a_vertex");
    glEnableVertexAttribArray(static_cast<GLuint>(a_vertex));

    glVertexAttribPointer(static_cast<GLuint>(a_vertex), 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    u_transform = glGetUniformLocation(m_program, "modelViewProjection");
    u_time = glGetUniformLocation(m_program, "time");

    m_time = clock::now();

    GLint terrain = glGetUniformLocation(m_program, "terrain");
    GLint patches = glGetUniformLocation(m_program, "patches");

    // since only single program and single data is used, bind only once 

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    
    glClearColor(0.f, 0.f, 0.f, 1.0f);

    glUseProgram(m_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]);
    glUniform1i(terrain, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]);
    glUniform1i(patches, 1);

    // view

    m_view = mat4::lookAt(0.f, 4.f,-4.f, 0.f, -0.6f, 0.f, 0.f, 1.f, 0.f);
}

CubeScape::~CubeScape()
{
    glDeleteBuffers(1, &m_vertices);
    glDeleteBuffers(1, &m_indices);

    glDeleteProgram(m_program);
}

void CubeScape::resize(int width, int height)
{
    m_projection = mat4::perspective(40.f, static_cast<GLfloat>(width) / static_cast<GLfloat>(height), 2.f, 8.f);

    glViewport(0, 0, width, height);
}

void CubeScape::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - m_time);
    float t = static_cast<float>(ms.count()) * 1e-3f;

    const mat4 transform = m_projection * m_view * mat4::rotate(t * 0.1f, 0.f, 1.f, 0.f);

    glUniformMatrix4fv(u_transform, 1, GL_FALSE, &transform[0]);
    glUniform1f(u_time, t);

    glDrawElementsInstanced(GL_TRIANGLES, 18, GL_UNSIGNED_BYTE, 0, 32 * 32);
}