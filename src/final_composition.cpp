#include "final_composition.h"
#include "global_graphics_resources.h"
#include "logger.h"
#include "constants.h"

// -----------------------------------------------------------------------------------------------------------------------------------

FinalComposition::FinalComposition()
{
	std::string vs_path = "shader/quad_vs.glsl";
	m_composition_vs = GlobalGraphicsResources::load_shader(GL_VERTEX_SHADER, vs_path, nullptr);

	std::string fs_path = "shader/quad_fs.glsl";
	m_composition_fs = GlobalGraphicsResources::load_shader(GL_FRAGMENT_SHADER, fs_path, nullptr);

	dw::Shader* shaders[] = { m_composition_vs, m_composition_fs };

	m_composition_program = GlobalGraphicsResources::load_program(vs_path + fs_path, 2, &shaders[0]);

	if (!m_composition_vs || !m_composition_fs || !m_composition_program)
	{
		DW_LOG_INFO("Failed to load Composition pass shaders");
	}
}

// -----------------------------------------------------------------------------------------------------------------------------------

FinalComposition::~FinalComposition() {}

// -----------------------------------------------------------------------------------------------------------------------------------

void FinalComposition::render(dw::Camera* camera, uint32_t w, uint32_t h)
{
	m_composition_program->use();

	m_composition_program->set_uniform("u_CurrentOutput", 0);
	m_composition_program->set_uniform("u_NearPlane", camera->m_near);
	m_composition_program->set_uniform("u_FarPlane", camera->m_far);

	GlobalGraphicsResources::lookup_texture(RENDER_TARGET_COLOR)->bind(0);
	m_composition_program->set_uniform("s_Color", 0);

	GlobalGraphicsResources::lookup_texture(RENDER_TARGET_DEPTH)->bind(1);
	m_composition_program->set_uniform("s_Depth", 1);

	m_post_process_renderer.render(w, h, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------