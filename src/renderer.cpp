#include "renderer.h"
#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "logger.h"
#include "utility.h"
#include "imgui.h"
#include "entity.h"
#include "global_graphics_resources.h"
#include "constants.h"
#include "profiler.h"
#include "render_graph.h"
#include "geometry.h"
#include "profiler.h"

#include <gtc/matrix_transform.hpp>
#include <fstream>

namespace nimble
{
	static const uint32_t kDirectionalLightShadowMapSizes[] =
	{
		512,
		1024,
		2048,
		4096
	};

	static const uint32_t kSpotLightShadowMapSizes[] =
	{
		512,
		1024,
		2048,
		4096
	};

	static const uint32_t kPointShadowMapSizes[] =
	{
		256,
		512,
		1024,
		2048
	};

	// -----------------------------------------------------------------------------------------------------------------------------------

	Renderer::Renderer(Settings settings) : m_settings(settings) {}

	// -----------------------------------------------------------------------------------------------------------------------------------

	Renderer::~Renderer() {}

	// -----------------------------------------------------------------------------------------------------------------------------------

	bool Renderer::initialize()
	{
		m_directional_light_shadow_maps.reset();
		m_spot_light_shadow_maps.reset();
		m_point_light_shadow_maps.reset();

		// Create shadow maps
		m_directional_light_shadow_maps = GlobalGraphicsResources::request_general_render_target(kDirectionalLightShadowMapSizes[m_settings.shadow_map_quality], kDirectionalLightShadowMapSizes[m_settings.shadow_map_quality], GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 1, m_settings.cascade_count * MAX_SHADOW_CASTING_DIRECTIONAL_LIGHTS);
		m_spot_light_shadow_maps = GlobalGraphicsResources::request_general_render_target(kSpotLightShadowMapSizes[m_settings.shadow_map_quality], kSpotLightShadowMapSizes[m_settings.shadow_map_quality], GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 1, MAX_SHADOW_CASTING_SPOT_LIGHTS);
		m_point_light_shadow_maps = GlobalGraphicsResources::request_general_render_target(kPointShadowMapSizes[m_settings.shadow_map_quality], kPointShadowMapSizes[m_settings.shadow_map_quality], GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 1, MAX_SHADOW_CASTING_POINT_LIGHTS);

		// Create shadow map Render Target Views
		for (uint32_t i = 0; i < MAX_SHADOW_CASTING_DIRECTIONAL_LIGHTS; i++)
		{
			for (uint32_t j = 0; j < m_settings.cascade_count; j++)
				m_directionl_light_rt_views.push_back({ 0, i * m_settings.cascade_count + j, 0, m_directional_light_shadow_maps.get() });
		}

		for (uint32_t i = 0; i < MAX_SHADOW_CASTING_SPOT_LIGHTS; i++)
			m_spot_light_rt_views.push_back({ 0, i, 0, m_spot_light_shadow_maps.get() });

		for (uint32_t i = 0; i < MAX_SHADOW_CASTING_POINT_LIGHTS; i++)
		{
			for (uint32_t j = 0; j < 6; j++)
				m_point_light_rt_views.push_back({ j, i, 0, m_point_light_shadow_maps.get() });
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::render()
	{
		queue_default_views();

		update_uniforms();

		cull_scene();

		render_all_views();

		clear_all_views();
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::shutdown()
	{
		m_directional_light_shadow_maps.reset();
		m_spot_light_shadow_maps.reset();
		m_point_light_shadow_maps.reset();
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::set_settings(Settings settings)
	{
		m_settings = settings;
		initialize();
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::set_scene(std::shared_ptr<Scene> scene)
	{
		m_scene = scene;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::set_scene_render_graph(RenderGraph* graph)
	{
		if (graph)
		{
			m_scene_render_graph = graph;

			if (!m_scene_render_graph->initialize())
				NIMBLE_LOG_ERROR("Failed to initialize Scene Render Graph!");
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::queue_view(View view)
	{
		if (m_num_active_views == MAX_VIEWS)
			NIMBLE_LOG_ERROR("Maximum number of Views reached (64)");
		else
		{
			uint32_t idx = m_num_active_views++;

			Frustum frustum;
			frustum_from_matrix(frustum, view.m_vp_mat);

			m_active_views[idx] = view;
			m_active_frustums[idx] = frustum;
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::push_directional_light_views(View& dependent_view)
	{
		if (m_scene)
		{
			uint32_t shadow_casting_light_idx = 0;
			DirectionalLight* lights = m_scene->directional_lights();

			for (uint32_t light_idx = 0; light_idx < m_scene->directional_light_count(); light_idx++)
			{
				DirectionalLight& light = lights[light_idx];

				if (light.casts_shadow)
				{
					for (uint32_t cascade_idx = 0; cascade_idx < m_settings.cascade_count; cascade_idx++)
					{
						View light_view;

						light_view.m_enabled = true;
						light_view.m_culling = true;
						light_view.m_direction = light.transform.forward();
						light_view.m_position = light.transform.position;
						light_view.m_view_mat = glm::mat4(1.0f); // @TODO
						light_view.m_projection_mat = glm::mat4(1.0f); // @TODO
						light_view.m_vp_mat = glm::mat4(1.0f); // @TODO
						light_view.m_prev_vp_mat = glm::mat4(1.0f); // @TODO
						light_view.m_inv_view_mat = glm::mat4(1.0f); // @TODO
						light_view.m_inv_projection_mat = glm::mat4(1.0f); // @TODO
						light_view.m_inv_vp_mat = glm::mat4(1.0f); // @TODO
						light_view.m_jitter = glm::vec4(0.0);
						light_view.m_dest_render_target_view = &m_directionl_light_rt_views[shadow_casting_light_idx * m_settings.cascade_count + cascade_idx];
						light_view.m_graph = m_shadow_map_render_graph;
						light_view.m_scene = m_scene.get();

						queue_view(light_view);
					}

					shadow_casting_light_idx++;
				}	

				// Stop adding views if max number of shadow casting lights is already queued.
				if (shadow_casting_light_idx == (MAX_SHADOW_CASTING_DIRECTIONAL_LIGHTS - 1))
					break;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::push_spot_light_views()
	{

	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::push_point_light_views()
	{

	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::clear_all_views()
	{
		m_num_active_views = 0;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::on_window_resized(const uint32_t& w, const uint32_t& h)
	{
		GlobalGraphicsResources::initialize_render_targets(w, h);

		if (m_scene_render_graph)
			m_scene_render_graph->on_window_resized(w, h);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::update_uniforms()
	{
		if (m_scene)
		{
			// Update per entity uniforms
			Entity* entities = m_scene->entities();

			for (uint32_t i = 0; i < m_scene->entity_count(); i++)
			{
				Entity& entity = entities[i];

				m_per_entity_uniforms[i].modal_mat = entity.transform.model;
				m_per_entity_uniforms[i].last_model_mat = entity.transform.prev_model;
				m_per_entity_uniforms[i].world_pos = glm::vec4(entity.transform.position, 0.0f);
			}

			void* ptr = GlobalGraphicsResources::per_entity_ubo()->map(GL_WRITE_ONLY);
			memcpy(ptr, &m_per_entity_uniforms[0], sizeof(PerEntityUniforms) * m_scene->entity_count());
			GlobalGraphicsResources::per_entity_ubo()->unmap();

			// Update per view uniforms
			for (uint32_t i = 0; i < m_num_active_views; i++)
			{
				View& view = m_active_views[i];

				m_per_view_uniforms[i].view_mat = view.m_view_mat;
				m_per_view_uniforms[i].proj_mat = view.m_projection_mat;
				m_per_view_uniforms[i].view_proj= view.m_vp_mat;
				m_per_view_uniforms[i].last_view_proj = view.m_prev_vp_mat;
				m_per_view_uniforms[i].inv_proj = view.m_inv_projection_mat;
				m_per_view_uniforms[i].inv_view = view.m_inv_view_mat;
				m_per_view_uniforms[i].inv_view_proj = view.m_inv_vp_mat;
				m_per_view_uniforms[i].view_pos = glm::vec4(view.m_position, 0.0f);
			}

			ptr = GlobalGraphicsResources::per_view_ubo()->map(GL_WRITE_ONLY);
			memcpy(ptr, &m_per_view_uniforms[0], sizeof(PerViewUniforms) * m_num_active_views);
			GlobalGraphicsResources::per_view_ubo()->unmap();

			// Update per scene uniforms
			DirectionalLight* dir_lights = m_scene->directional_lights();

			m_per_scene_uniforms.directional_light_count = m_scene->directional_light_count();

			for (uint32_t light_idx = 0; light_idx < m_per_scene_uniforms.directional_light_count; light_idx++)
			{
				DirectionalLight& light = dir_lights[light_idx];

				m_per_scene_uniforms.directional_lights[light_idx].direction = glm::vec4(light.transform.forward(), 0.0f);
				m_per_scene_uniforms.directional_lights[light_idx].color_intensity = glm::vec4(light.color, light.intensity);
				m_per_scene_uniforms.directional_lights[light_idx].casts_shadow = light.casts_shadow ? 1 : 0;
			}

			SpotLight* spot_lights = m_scene->spot_lights();

			m_per_scene_uniforms.spot_light_count = m_scene->spot_light_count();

			for (uint32_t light_idx = 0; light_idx < m_per_scene_uniforms.spot_light_count; light_idx++)
			{
				SpotLight& light = spot_lights[light_idx];

				m_per_scene_uniforms.spot_lights[light_idx].direction_range = glm::vec4(light.transform.forward(), light.range);
				m_per_scene_uniforms.spot_lights[light_idx].color_intensity = glm::vec4(light.color, light.intensity);
				m_per_scene_uniforms.spot_lights[light_idx].position_cone_angle = glm::vec4(light.transform.position, light.cone_angle);
				m_per_scene_uniforms.spot_lights[light_idx].casts_shadow = light.casts_shadow ? 1 : 0;
			}

			PointLight* point_lights = m_scene->point_lights();

			m_per_scene_uniforms.point_light_count = m_scene->point_light_count();

			for (uint32_t light_idx = 0; light_idx < m_per_scene_uniforms.point_light_count; light_idx++)
			{
				PointLight& light = point_lights[light_idx];

				m_per_scene_uniforms.point_lights[light_idx].position_range = glm::vec4(light.transform.position, light.range);
				m_per_scene_uniforms.point_lights[light_idx].color_intensity = glm::vec4(light.color, light.intensity);
				m_per_scene_uniforms.point_lights[light_idx].casts_shadow = light.casts_shadow ? 1 : 0;
			}

			ptr = GlobalGraphicsResources::per_scene_ssbo()->map(GL_WRITE_ONLY);
			memcpy(ptr, &m_per_scene_uniforms, sizeof(PerSceneUniforms));
			GlobalGraphicsResources::per_scene_ssbo()->unmap();
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::cull_scene()
	{
		Profiler::begin_cpu_sample(PROFILER_FRUSTUM_CULLING);
		 
		if (m_scene)
		{
			Entity* entities = m_scene->entities();

			for (uint32_t i = 0; i < m_scene->entity_count(); i++)
			{
				Entity& entity = entities[i];

				entity.obb.position = entity.transform.position;
				entity.obb.orientation = glm::mat3(entity.transform.model);
				
				for (uint32_t j = 0; j < m_num_active_views; j++)
				{
					if (m_active_views[j].m_culling)
					{
						if (intersects(m_active_frustums[j], entity.obb))
						{
							entity.set_visible(j);

#ifdef ENABLE_SUBMESH_CULLING
							for (uint32_t k = 0; k < entity.mesh->submesh_count(); k++)
							{
								SubMesh& submesh = entity.mesh->submesh(k);
								glm::vec3 center = (submesh.min_extents + submesh.max_extents) / 2.0f;

								entity.submesh_spheres[k].position = center + entity.transform.position;

								if (intersects(m_active_frustums[j], entity.submesh_spheres[k]))
									entity.set_submesh_visible(k, j);
								else
									entity.set_submesh_invisible(k, j);
							}
#endif
						}
						else
							entity.set_invisible(j);
					}
					else
						entity.set_visible(j);
				}
			}
		}

		Profiler::end_cpu_sample(PROFILER_FRUSTUM_CULLING);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::queue_default_views()
	{
		if (m_scene)
		{
			// Allocate view for scene camera
			auto camera = m_scene->camera();
			View scene_view;

			scene_view.m_enabled = true;
			scene_view.m_culling = true;
			scene_view.m_direction = camera->m_forward;
			scene_view.m_position = camera->m_position;
			scene_view.m_view_mat = camera->m_view;
			scene_view.m_projection_mat = camera->m_projection;
			scene_view.m_vp_mat = camera->m_view_projection;
			scene_view.m_prev_vp_mat = camera->m_prev_view_projection;
			scene_view.m_inv_view_mat = glm::inverse(camera->m_view);
			scene_view.m_inv_projection_mat = glm::inverse(camera->m_projection);
			scene_view.m_inv_vp_mat = glm::inverse(camera->m_view_projection);
			scene_view.m_jitter = glm::vec4(camera->m_prev_jitter, camera->m_current_jitter);
			scene_view.m_dest_render_target_view = nullptr;
			scene_view.m_graph = m_scene_render_graph;
			scene_view.m_scene = m_scene.get();

			// @TODO: Create shadow views for scene views

			// Finally queue the scene view
			queue_view(scene_view);
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	void Renderer::render_all_views()
	{
		for (uint32_t i = 0; i < m_num_active_views; i++)
		{
			View& view = m_active_views[i];

			if (view.m_enabled)
			{
				view.m_id = i;

				if (view.m_graph)
					view.m_graph->execute(view);
				else
					NIMBLE_LOG_ERROR("Render Graph not assigned for View!");
			}
		}
	}
	
	// -----------------------------------------------------------------------------------------------------------------------------------
}