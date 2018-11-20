#include "resource_manager.h"
#include "logger.h"
#include "ogl.h"
#include "material.h"
#include "mesh.h"
#include <runtime/loader.h>

namespace nimble
{
	// -----------------------------------------------------------------------------------------------------------------------------------

	static const GLenum kInternalFormatTable[][4] =
	{
		{ GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
		{ GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
		{ GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F }
	};

	static const GLenum kCompressedTable[][2] =
	{
		{ GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT }, // BC1
		{ GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT }, // BC1a
		{ GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT }, // BC2
		{ GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT }, // BC3
		{ GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT }, // BC3n
		{ GL_COMPRESSED_RED_RGTC1, 0 }, // BC4
		{ GL_COMPRESSED_RG_RGTC2, 0 }, // BC5
		{ GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB, 0 }, // BC6
		{ GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB } // BC7
	};

	static const GLenum kFormatTable[] =
	{
		GL_RED,
		GL_RG,
		GL_RGB,
		GL_RGBA
	};

	static const GLenum kTypeTable[] =
	{
		GL_UNSIGNED_BYTE,
		GL_HALF_FLOAT,
		GL_FLOAT
	};

	static const TextureType kTextureTypeTable[] = 
	{
		TEXTURE_TYPE_EMISSIVE,
		TEXTURE_TYPE_DISPLACEMENT,
		TEXTURE_TYPE_NORMAL,
		TEXTURE_TYPE_METAL_SPEC,
		TEXTURE_TYPE_ROUGH_SMOOTH,
		TEXTURE_TYPE_METAL_SPEC,
		TEXTURE_TYPE_ROUGH_SMOOTH,
		TEXTURE_TYPE_CUSTOM
	};

	// -----------------------------------------------------------------------------------------------------------------------------------

	ResourceManager::ResourceManager()
	{

	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	ResourceManager::~ResourceManager()
	{

	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	std::shared_ptr<Texture> ResourceManager::load_texture(const std::string& path, const bool& srgb, const bool& cubemap)
	{
		if (m_texture_cache.find(path) != m_texture_cache.end() && m_texture_cache[path].lock())
			return m_texture_cache[path].lock();
		else
		{
			ast::Image image;

			if (ast::load_image(path, image))
			{
				uint32_t type = 0;

				if (image.type == ast::PIXEL_TYPE_FLOAT16)
					type = 1;
				else if (image.type == ast::PIXEL_TYPE_FLOAT32)
					type = 2;

				if (cubemap)
				{
					if (image.array_slices != 6)
					{
						NIMBLE_LOG_ERROR("Texture does not have 6 array slices: " + path);
						return nullptr;
					}

					if (image.compression == ast::COMPRESSION_NONE)
					{
						std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>(image.data[0][0].width,
							image.data[0][0].height,
							image.array_slices,
							image.mip_slices,
							kInternalFormatTable[type][image.components - 1],
							kFormatTable[image.components - 1],
							kTypeTable[type]);

						for (uint32_t i = 0; i < image.array_slices; i++)
						{
							for (uint32_t j = 0; j < image.mip_slices; j++)
								texture->set_data(i, 0, j, image.data[i][j].data);
						}

						m_texture_cache[path] = texture;

						return texture;
					}
					else
					{
						if (kCompressedTable[image.compression - 1][(int)srgb] == 0)
						{
							NIMBLE_LOG_ERROR("No SRGB format available for this compression type: " + path);
							return nullptr;
						}

						std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>(image.data[0][0].width,
							image.data[0][0].height,
							1,
							image.mip_slices,
							kCompressedTable[image.compression - 1][(int)srgb],
							kFormatTable[image.components - 1],
							kTypeTable[type],
							true);

						for (uint32_t i = 0; i < image.array_slices; i++)
						{
							for (uint32_t j = 0; j < image.mip_slices; j++)
								texture->set_compressed_data(i, 0, j, image.data[i][j].size, image.data[i][j].data);
						}

						m_texture_cache[path] = texture;

						return texture;
					}
				}
				else
				{
					if (image.compression == ast::COMPRESSION_NONE)
					{
						std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>(image.data[0][0].width,
							image.data[0][0].height,
							image.array_slices,
							image.mip_slices,
							1,
							kInternalFormatTable[type][image.components - 1],
							kFormatTable[image.components - 1],
							kTypeTable[type]);

						for (uint32_t i = 0; i < image.array_slices; i++)
						{
							for (uint32_t j = 0; j < image.mip_slices; j++)
								texture->set_data(i, j, image.data[i][j].data);
						}

						m_texture_cache[path] = texture;

						return texture;
					}
					else
					{
						std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>(image.data[0][0].width,
							image.data[0][0].height,
							image.array_slices,
							image.mip_slices,
							1,
							kCompressedTable[image.compression - 1][(int)srgb],
							kFormatTable[image.components - 1],
							kTypeTable[type],
							true);

						for (uint32_t i = 0; i < image.array_slices; i++)
						{
							for (uint32_t j = 0; j < image.mip_slices; j++)
								texture->set_compressed_data(i, j, image.data[i][j].size, image.data[i][j].data);
						}

						m_texture_cache[path] = texture;

						return texture;
					}
				}
			}
			else
			{
				NIMBLE_LOG_ERROR("Failed to load Texture: " + path);
				return nullptr;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	std::shared_ptr<Material> ResourceManager::load_material(const std::string& path)
	{
		if (m_material_cache.find(path) != m_material_cache.end() && m_material_cache[path].lock())
			return m_material_cache[path].lock();
		else
		{
			ast::Material ast_material;

			if (ast::load_material(path, ast_material))
			{
				std::shared_ptr<Material> material = std::make_shared<Material>();

				material->set_name(ast_material.name);
				material->set_metallic_workflow(ast_material.metallic_workflow);
				material->set_double_sided(ast_material.double_sided);
				material->set_vertex_shader_func(ast_material.vertex_shader_func);
				material->set_fragment_shader_func(ast_material.fragment_shader_func);
				material->set_blend_mode((BlendMode)ast_material.blend_mode);
				material->set_displacement_type((DisplacementType)ast_material.displacement_type);
				material->set_shading_model((ShadingModel)ast_material.shading_model);
				material->set_lighting_model((LightingModel)ast_material.lighting_model);

				uint32_t custom_texture_count = 0;

				for (const auto& texture_desc : ast_material.textures)
				{
					if (texture_desc.type == ast::TEXTURE_CUSTOM)
						custom_texture_count++;
					else
						material->set_surface_texture(kTextureTypeTable[texture_desc.type], load_texture(texture_desc.path, texture_desc.srgb));
				}

				// Iterate a second time to add the custom textures
				for (uint32_t i = 0; i < ast_material.textures.size(); i++)
					material->set_custom_texture(i, load_texture(ast_material.textures[i].path, ast_material.textures[i].srgb));

				m_material_cache[path] = material;

				return material;
			}
			else
			{
				NIMBLE_LOG_ERROR("Failed to load Material: " + path);
				return nullptr;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& path)
	{
		if (m_mesh_cache.find(path) != m_mesh_cache.end() && m_mesh_cache[path].lock())
			return m_mesh_cache[path].lock();
		else
		{
			ast::Mesh ast_mesh;

			if (ast::load_mesh(path, ast_mesh))
			{
				VertexArray* vao = nullptr;
				VertexBuffer* vbo = nullptr;
				IndexBuffer* ibo = nullptr;

				vbo = new VertexBuffer(GL_STATIC_DRAW, sizeof(ast::Vertex) * ast_mesh.vertices.size(), (void*)&ast_mesh.vertices[0]);

				if (!vbo)
					NIMBLE_LOG_ERROR("Failed to create Vertex Buffer");

				// Create index buffer.
				ibo = new IndexBuffer(GL_STATIC_DRAW, sizeof(uint32_t) * ast_mesh.indices.size(), (void*)ast_mesh.indices[0]);

				if (!ibo)
					NIMBLE_LOG_ERROR("Failed to create Index Buffer");

				// Declare vertex attributes.
				VertexAttrib attribs[] =
				{
					{ 3, GL_FLOAT, false, 0 },
					{ 2, GL_FLOAT, false, offsetof(ast::Vertex, tex_coord) },
					{ 3, GL_FLOAT, false, offsetof(ast::Vertex, normal) },
					{ 3, GL_FLOAT, false, offsetof(ast::Vertex, tangent) },
					{ 3, GL_FLOAT, false, offsetof(ast::Vertex, bitangent) }
				};

				// Create vertex array.
				vao = new VertexArray(vbo, ibo, sizeof(ast::Vertex), 5, attribs);

				if (!vao)
					NIMBLE_LOG_ERROR("Failed to create Vertex Array");

				std::vector<std::shared_ptr<Material>> materials;
				materials.resize(ast_mesh.materials.size());

				for (uint32_t i = 0; i < ast_mesh.materials.size(); i++)
					materials[i] = load_material(ast_mesh.material_paths[i]);

				std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>(ast_mesh.name, ast_mesh.max_extents, ast_mesh.min_extents, ast_mesh.submeshes, materials, vbo, ibo, vao);

				m_mesh_cache[path] = mesh;

				return mesh;
			}
			else
			{
				NIMBLE_LOG_ERROR("Failed to load Mesh: " + path);
				return nullptr;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	std::shared_ptr<Scene> ResourceManager::load_scene(const std::string& path)
	{
		if (m_scene_cache.find(path) != m_scene_cache.end() && m_scene_cache[path].lock())
			return m_scene_cache[path].lock();
		else
		{
			ast::Scene ast_scene;

			if (ast::load_scene(path, ast_scene))
			{
			}
			else
			{
				NIMBLE_LOG_ERROR("Failed to load Scene: " + path);
				return nullptr;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
}