#include "motion_blur.h"
#include "global_graphics_resources.h"
#include "logger.h"
#include "constants.h"
#include "gpu_profiler.h"
#include "imgui.h"

namespace nimble
{
	// -----------------------------------------------------------------------------------------------------------------------------------

	MotionBlur::MotionBlur() {}

	// -----------------------------------------------------------------------------------------------------------------------------------

	MotionBlur::~MotionBlur() {}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void MotionBlur::initialize(uint16_t width, uint16_t height)
	{
		on_window_resized(width, height);

		std::string vs_path = "shader/post_process/quad_vs.glsl";
		m_motion_blur_vs = GlobalGraphicsResources::load_shader(GL_VERTEX_SHADER, vs_path);

		std::string fs_path = "shader/post_process/motion_blur/motion_blur_fs.glsl";
		m_motion_blur_fs = GlobalGraphicsResources::load_shader(GL_FRAGMENT_SHADER, fs_path);

		Shader* shaders[] = { m_motion_blur_vs, m_motion_blur_fs };
		std::string combined_path = vs_path + fs_path;
		m_motion_blur_program = GlobalGraphicsResources::load_program(combined_path, 2, &shaders[0]);

		if (!m_motion_blur_vs || !m_motion_blur_fs || !m_motion_blur_program)
		{
			NIMBLE_LOG_ERROR("Failed to load Motion blur pass shaders");
		}

		m_motion_blur_program->uniform_block_binding("u_PerFrame", 0);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void MotionBlur::shutdown() {}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void MotionBlur::profiling_gui()
	{
		ImGui::Text("Motion Blur: %f ms", GPUProfiler::result("MotionBlur"));
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void MotionBlur::on_window_resized(uint16_t width, uint16_t height)
	{
		// Clear earlier render targets.
		GlobalGraphicsResources::destroy_framebuffer(FRAMEBUFFER_MOTION_BLUR);
		GlobalGraphicsResources::destroy_texture(RENDER_TARGET_MOTION_BLUR);

		// Create Render targets.
		m_motion_blur_rt = GlobalGraphicsResources::create_texture_2d(RENDER_TARGET_MOTION_BLUR, width, height, GL_RGB32F, GL_RGB, GL_FLOAT);
		m_motion_blur_rt->set_min_filter(GL_LINEAR);
		m_motion_blur_rt->set_wrapping(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		// Create FBO.
		m_motion_blur_fbo = GlobalGraphicsResources::create_framebuffer(FRAMEBUFFER_MOTION_BLUR);

		// Attach render target to FBO.
		m_motion_blur_fbo->attach_render_target(0, m_motion_blur_rt, 0, 0);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void MotionBlur::render(uint32_t w, uint32_t h)
	{
		GPUProfiler::begin("MotionBlur");

		m_motion_blur_program->use();

		// Bind global UBO's.
		GlobalGraphicsResources::per_frame_ubo()->bind_base(0);

		if (m_motion_blur_program->set_uniform("s_ColorMap", 0))
			GlobalGraphicsResources::lookup_texture(RENDER_TARGET_DOF_COMPOSITE)->bind(0);

		// Bind Textures.
		PerFrameUniforms& per_frame = GlobalGraphicsResources::per_frame_uniforms();

		if (per_frame.renderer == RENDERER_FORWARD)
		{
			//if (m_motion_blur_program->set_uniform("s_ColorMap", 0))
			//	GlobalGraphicsResources::lookup_texture(RENDER_TARGET_FORWARD_COLOR)->bind(0);

			if (m_motion_blur_program->set_uniform("s_VelocityMap", 1))
				GlobalGraphicsResources::lookup_texture(RENDER_TARGET_FORWARD_VELOCITY)->bind(1);
		}
		else if (per_frame.renderer == RENDERER_DEFERRED)
		{
			//if (m_motion_blur_program->set_uniform("s_ColorMap", 0))
			//	GlobalGraphicsResources::lookup_texture(RENDER_TARGET_DEFERRED_COLOR)->bind(0);

			if (m_motion_blur_program->set_uniform("s_VelocityMap", 1))
				GlobalGraphicsResources::lookup_texture(RENDER_TARGET_GBUFFER_RT1)->bind(1);
		}
		
		m_post_process_renderer.render(w, h, m_motion_blur_fbo);

		GPUProfiler::end("MotionBlur");
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
}