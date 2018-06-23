#include "global_graphics_resources.h"
#include "demo_loader.h"
#include "uniforms.h"
#include "constants.h"
#include "utility.h"

#include <logger.h>
#include <macros.h>

std::unordered_map<std::string, dw::Texture*> GlobalGraphicsResources::m_texture_map;
std::unordered_map<std::string, dw::Framebuffer*> GlobalGraphicsResources::m_framebuffer_map;
std::unordered_map<std::string, dw::Program*> GlobalGraphicsResources::m_program_cache;
std::unordered_map<std::string, dw::Shader*> GlobalGraphicsResources::m_shader_cache;
dw::VertexArray*   GlobalGraphicsResources::m_quad_vao = nullptr;
dw::VertexBuffer*  GlobalGraphicsResources::m_quad_vbo = nullptr;
dw::VertexArray*   GlobalGraphicsResources::m_cube_vao = nullptr;
dw::VertexBuffer*  GlobalGraphicsResources::m_cube_vbo = nullptr;
dw::UniformBuffer* GlobalGraphicsResources::m_per_frame = nullptr;
dw::UniformBuffer* GlobalGraphicsResources::m_per_scene = nullptr;
dw::UniformBuffer* GlobalGraphicsResources::m_per_entity = nullptr;

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::initialize()
{
	// Load BRDF look-up-texture.
	dw::Texture* brdf_lut = demo::load_image("texture/brdfLUT.trm", GL_RG16F, GL_RG, GL_HALF_FLOAT);
	brdf_lut->set_min_filter(GL_LINEAR);
	brdf_lut->set_mag_filter(GL_LINEAR);

	m_texture_map[BRDF_LUT] = brdf_lut;

	// Create uniform buffers.
	m_per_frame = new dw::UniformBuffer(GL_DYNAMIC_DRAW, sizeof(PerFrameUniforms));
	m_per_scene = new dw::UniformBuffer(GL_DYNAMIC_DRAW, sizeof(PerSceneUniforms));
	m_per_entity = new dw::UniformBuffer(GL_DYNAMIC_DRAW, 1024 * sizeof(PerEntityUniforms));

	// Create common geometry VBO's and VAO's.
	create_quad();
	create_cube();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::shutdown()
{
	// Delete common geometry VBO's and VAO's.
	DW_SAFE_DELETE(m_quad_vao);
	DW_SAFE_DELETE(m_quad_vbo);
	DW_SAFE_DELETE(m_cube_vao);
	DW_SAFE_DELETE(m_cube_vbo);

	// Delete uniform buffers.
	DW_SAFE_DELETE(m_per_frame);
	DW_SAFE_DELETE(m_per_scene);
	DW_SAFE_DELETE(m_per_entity);

	// Delete framebuffers.
	for (auto itr : m_framebuffer_map)
	{
		DW_SAFE_DELETE(itr.second);
	}

	// Delete textures.
	for (auto itr : m_texture_map)
	{
		DW_SAFE_DELETE(itr.second);
	}

	// Delete programs.
	for (auto itr : m_program_cache)
	{
		DW_SAFE_DELETE(itr.second);
	}

	// Delete shaders.
	for (auto itr : m_shader_cache)
	{
		DW_SAFE_DELETE(itr.second);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Texture* GlobalGraphicsResources::lookup_texture(const std::string& name)
{
	if (m_texture_map.find(name) == m_texture_map.end())
		return nullptr;
	else
		return m_texture_map[name];
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Texture2D* GlobalGraphicsResources::create_texture_2d(const std::string& name, const uint32_t& w, const uint32_t& h, GLenum internal_format, GLenum format, GLenum type, uint32_t num_samples, uint32_t array_size, uint32_t mip_levels)
{
	if (m_texture_map.find(name) == m_texture_map.end())
	{
		dw::Texture2D* texture = new dw::Texture2D(w, h, array_size, mip_levels, num_samples, internal_format, format, type);
		m_texture_map[name] = texture;

		return texture;
	}
	else
	{
		DW_LOG_ERROR("A texture with the requested name (" + name + ") already exists. Returning nullptr...");
		return nullptr;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::TextureCube* GlobalGraphicsResources::create_texture_cube(const std::string& name, const uint32_t& w, const uint32_t& h, GLenum internal_format, GLenum format, GLenum type, uint32_t array_size, uint32_t mip_levels)
{
	if (m_texture_map.find(name) == m_texture_map.end())
	{
		dw::TextureCube* texture = new dw::TextureCube(w, h, array_size, mip_levels, internal_format, format, type);
		m_texture_map[name] = texture;

		return texture;
	}
	else
	{
		DW_LOG_ERROR("A texture with the requested name (" + name + ") already exists. Returning nullptr...");
		return nullptr;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::destroy_texture(const std::string& name)
{
	if (m_texture_map.find(name) != m_texture_map.end())
	{
		dw::Texture* texture = m_texture_map[name];
		DW_SAFE_DELETE(texture);
		m_texture_map.erase(name);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Framebuffer* GlobalGraphicsResources::lookup_framebuffer(const std::string& name)
{
	if (m_framebuffer_map.find(name) == m_framebuffer_map.end())
		return nullptr;
	else
		return m_framebuffer_map[name];
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Framebuffer* GlobalGraphicsResources::create_framebuffer(const std::string& name)
{
	if (m_framebuffer_map.find(name) == m_framebuffer_map.end())
	{
		dw::Framebuffer* fbo = new dw::Framebuffer();
		m_framebuffer_map[name] = fbo;

		return fbo;
	}
	else
	{
		DW_LOG_ERROR("A framebuffer with the requested name (" + name + ") already exists. Returning nullptr...");
		return nullptr;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::destroy_framebuffer(const std::string& name)
{
	if (m_framebuffer_map.find(name) != m_framebuffer_map.end())
	{
		dw::Framebuffer* fbo = m_framebuffer_map[name];
		DW_SAFE_DELETE(fbo);
		m_framebuffer_map.erase(name);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Shader* GlobalGraphicsResources::load_shader(GLuint type, std::string& path, dw::Material* mat)
{
	if (m_shader_cache.find(path) == m_shader_cache.end())
	{
		DW_LOG_INFO("Shader Asset not in cache. Loading from disk.");

		dw::Shader* shader = dw::Shader::create_from_file(type, dw::utility::path_for_resource("assets/" + path));
		m_shader_cache[path] = shader;
		return shader;
	}
	else
	{
		DW_LOG_INFO("Shader Asset already loaded. Retrieving from cache.");
		return m_shader_cache[path];
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

dw::Program* GlobalGraphicsResources::load_program(std::string& combined_name, uint32_t count, dw::Shader** shaders)
{
	if (m_program_cache.find(combined_name) == m_program_cache.end())
	{
		DW_LOG_INFO("Shader Program Asset not in cache. Loading from disk.");

		dw::Program* program = new dw::Program(count, shaders);
		m_program_cache[combined_name] = program;

		return program;
	}
	else
	{
		DW_LOG_INFO("Shader Program Asset already loaded. Retrieving from cache.");
		return m_program_cache[combined_name];
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::create_cube()
{
	float cube_vertices[] =
	{
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		// bottom face
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		// top face
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};

	m_cube_vbo = new dw::VertexBuffer(GL_STATIC_DRAW, sizeof(cube_vertices), (void*)cube_vertices);

	dw::VertexAttrib attribs[] =
	{
		{ 3,GL_FLOAT, false, 0, },
		{ 3,GL_FLOAT, false, sizeof(float) * 3 },
		{ 2,GL_FLOAT, false, sizeof(float) * 6 }
	};

	m_cube_vao = new dw::VertexArray(m_cube_vbo, nullptr, sizeof(float) * 8, 3, attribs);

	if (!m_cube_vbo || !m_cube_vao)
	{
		DW_LOG_FATAL("Failed to create Vertex Buffers/Arrays");
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

void GlobalGraphicsResources::create_quad()
{
	const float vertices[] =
	{
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f
	};

	m_quad_vbo = new dw::VertexBuffer(GL_STATIC_DRAW, sizeof(vertices), (void*)vertices);

	dw::VertexAttrib quad_attribs[] =
	{
		{ 3, GL_FLOAT, false, 0, },
		{ 2, GL_FLOAT, false, sizeof(float) * 3 }
	};

	m_quad_vao = new dw::VertexArray(m_quad_vbo, nullptr, sizeof(float) * 5, 2, quad_attribs);

	if (!m_quad_vbo || !m_quad_vao)
	{
		DW_LOG_INFO("Failed to create Vertex Buffers/Arrays");
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------
