#include "tr_local.h"

FT_DECLARE_SHADER( single_texture_vert );
FT_DECLARE_SHADER( single_texture_frag );

// FIXME: link it
#include "shader_single_texture_vert.c"
#include "shader_single_texture_frag.c"

extern struct ft_wsi_info SDL_wsiInfo;

ftContext_t ft;

static inline void CreatePipelines( void )
{
	struct ft_shader     *shader;
	struct ft_shader_info shader_info = {
	    .vertex   = get_single_texture_vert_shader( ft.renderer_api ),
	    .fragment = get_single_texture_frag_shader( ft.renderer_api ),
	};
	ft_create_shader( ft.device, &shader_info, &shader );

	ft_create_descriptor_set_layout( ft.device, shader, &ft.dsl_2d );

	struct ft_vertex_layout vertex_layout = {
	    .binding_info_count = 3, // 3 for 2d pipeline
	    .binding_infos =
	        {
	            [0] =
	                {
	                    .binding    = 0,
	                    .input_rate = FT_VERTEX_INPUT_RATE_VERTEX,
	                    .stride     = sizeof( vec4_t ),
	                },
	            [1] =
	                {
	                    .binding    = 1,
	                    .input_rate = FT_VERTEX_INPUT_RATE_VERTEX,
	                    .stride     = sizeof( color4ub_t ),
	                },
	            [2] =
	                {
	                    .binding    = 2,
	                    .input_rate = FT_VERTEX_INPUT_RATE_VERTEX,
	                    .stride     = sizeof( vec2_t ),
	                },
	            [3] =
	                {
	                    .binding    = 3,
	                    .input_rate = FT_VERTEX_INPUT_RATE_VERTEX,
	                    .stride     = sizeof( vec2_t ),
	                },
	        },
	    .attribute_info_count = 3, // 3 for 2d pipeline
	    .attribute_infos =
	        {
	            [0] =
	                {
	                    .binding  = 0,
	                    .location = 0,
	                    .offset   = 0,
	                    .format   = FT_FORMAT_R32G32B32A32_SFLOAT,
	                },
	            [1] =
	                {
	                    .binding  = 1,
	                    .location = 1,
	                    .offset   = 0,
	                    .format   = FT_FORMAT_R8G8B8A8_UNORM,
	                },
	            [2] =
	                {
	                    .binding  = 2,
	                    .location = 2,
	                    .offset   = 0,
	                    .format   = FT_FORMAT_R32G32_SFLOAT,
	                },
	            [3] =
	                {
	                    .binding  = 3,
	                    .location = 3,
	                    .offset   = 0,
	                    .format   = FT_FORMAT_R32G32_SFLOAT,
	                },
	        },
	};

	struct ft_pipeline_info pipeline_info = {
	    .name                  = "2d_pipeline",
	    .type                  = FT_PIPELINE_TYPE_GRAPHICS,
	    .shader                = shader,
	    .descriptor_set_layout = ft.dsl_2d,
	    .sample_count          = 1,
	    .topology              = FT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	    .vertex_layout         = vertex_layout,
	    .rasterizer_info =
	        {
	            .cull_mode    = FT_CULL_MODE_NONE,
	            .polygon_mode = FT_POLYGON_MODE_FILL,
	        },
	    .depth_state_info =
	        {
	            .depth_test  = false,
	            .depth_write = false,
	        },
	    .color_attachment_count      = 1,
	    .color_attachment_formats[0] = ft_get_swapchain_format( ft.swapchain ),
	    .depth_stencil_format        = FT_FORMAT_D32_SFLOAT,
	};

	ft_create_pipeline( ft.device, &pipeline_info, &ft.pipeline_2d );

	ft_destroy_shader( ft.device, shader );
}

void FT_Init( void )
{
	if ( !ft_log_init( FT_LOG_LEVEL_INFO, 0 ) )
	{
		ri.Printf( ERR_DROP, "ERROR: failed to initialize fluent renderer\n" );
		return;
	}

	if ( glConfig.vidWidth == 0 )
	{
		GLint temp;

		GLimp_Init( qfalse );

		glConfig.textureEnvAddAvailable = qtrue;

		// FIXME: fill proper
		glConfig.maxTextureSize          = 10000;
		glConfig.numTextureUnits         = 10000;
		temp                             = 100000;
		glRefConfig.glslMaxAnimatedBones = Com_Clamp( 0, IQM_MAX_JOINTS, ( temp - 160 ) / 16 );
		if ( glRefConfig.glslMaxAnimatedBones < 12 )
		{
			glRefConfig.glslMaxAnimatedBones = 0;
		}

		struct ft_instance_info instanceInfo = {
		    .api      = FT_RENDERER_API_VULKAN,
		    .wsi_info = &SDL_wsiInfo,
		};
		ft_create_instance( &instanceInfo, &ft.instance );

		struct ft_device_info deviceInfo;
		ft_create_device( ft.instance, &deviceInfo, &ft.device );

		struct ft_queue_info queueInfo = {
		    .queue_type = FT_QUEUE_TYPE_GRAPHICS,
		};
		ft_create_queue( ft.device, &queueInfo, &ft.queue );

		for ( uint32_t i = 0; i < FRAME_COUNT; i++ )
		{
			ft.frames[i].cmd_recorded = 0;
			ft_create_semaphore( ft.device, &ft.frames[i].present_semaphore );
			ft_create_semaphore( ft.device, &ft.frames[i].render_semaphore );
			ft_create_fence( ft.device, &ft.frames[i].render_fence );

			struct ft_command_pool_info pool_info = {
			    .queue = ft.queue,
			};

			ft_create_command_pool( ft.device, &pool_info, &ft.frames[i].cmd_pool );
			ft_create_command_buffers( ft.device, ft.frames[i].cmd_pool, 1, &ft.frames[i].cmd );
		}

		struct ft_swapchain_info swapchain_info = {
		    .width           = glConfig.vidWidth,
		    .height          = glConfig.vidHeight,
		    .format          = FT_FORMAT_B8G8R8A8_SRGB,
		    .min_image_count = FRAME_COUNT,
		    .vsync           = 1,
		    .queue           = ft.queue,
		    .wsi_info        = &SDL_wsiInfo,
		};
		ft_create_swapchain( ft.device, &swapchain_info, &ft.swapchain );

		struct ft_image_info depth_image_info = {
		    .width           = glConfig.vidWidth,
		    .height          = glConfig.vidHeight,
		    .depth           = 1,
		    .format          = FT_FORMAT_D32_SFLOAT,
		    .sample_count    = 1,
		    .layer_count     = 1,
		    .mip_levels      = 1,
		    .descriptor_type = FT_DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT,
		    .name            = "depth_image",
		};

		ft_create_image( ft.device, &depth_image_info, &ft.depth_image );

		CreatePipelines();
	}

	GL_SetDefaultState();
}

void FT_Shutdown( void )
{
	ft_queue_wait_idle( ft.queue );

	ft_destroy_pipeline( ft.device, ft.pipeline_2d );
	ft_destroy_descriptor_set_layout( ft.device, ft.dsl_2d );

	ft_destroy_image( ft.device, ft.depth_image );

	ft_destroy_swapchain( ft.device, ft.swapchain );

	for ( uint32_t i = 0; i < FRAME_COUNT; i++ )
	{
		ft_destroy_command_buffers( ft.device, ft.frames[i].cmd_pool, 1, &ft.frames[i].cmd );
		ft_destroy_command_pool( ft.device, ft.frames[i].cmd_pool );
		ft_destroy_fence( ft.device, ft.frames[i].render_fence );
		ft_destroy_semaphore( ft.device, ft.frames[i].render_semaphore );
		ft_destroy_semaphore( ft.device, ft.frames[i].present_semaphore );
	}

	ft_destroy_queue( ft.queue );
	ft_destroy_device( ft.device );
	ft_destroy_instance( ft.instance );
	ft_log_shutdown();
}

void FT_BeginFrame( void )
{
	if ( !ft.frames[ft.frame_index].cmd_recorded )
	{
		ft_wait_for_fences( ft.device, 1, &ft.frames[ft.frame_index].render_fence );
		ft_reset_fences( ft.device, 1, &ft.frames[ft.frame_index].render_fence );
		ft.frames[ft.frame_index].cmd_recorded = 1;
	}

	ft_acquire_next_image( ft.device, ft.swapchain, ft.frames[ft.frame_index].present_semaphore,
	                       NULL, &ft.image_index );

	ft_begin_command_buffer( ft.frames[ft.frame_index].cmd );
	ft.cmd = ft.frames[ft.frame_index].cmd;

	struct ft_image_barrier barriers[2] = {
	    [0] =
	        {
	            .image     = ft_get_swapchain_image( ft.swapchain, ft.image_index ),
	            .old_state = FT_RESOURCE_STATE_UNDEFINED,
	            .new_state = FT_RESOURCE_STATE_COLOR_ATTACHMENT,
	        },
	    [1] =
	        {
	            .image     = ft.depth_image,
	            .old_state = FT_RESOURCE_STATE_UNDEFINED,
	            .new_state = FT_RESOURCE_STATE_DEPTH_STENCIL_WRITE,
	        },
	};

	ft_cmd_barrier( ft.cmd, 0, NULL, 0, NULL, FT_COUNTOF( barriers ), barriers );

	struct ft_render_pass_begin_info rpbi = {
	    .width                  = glConfig.vidWidth,
	    .height                 = glConfig.vidHeight,
	    .color_attachment_count = 1,
	    .color_attachments[0] =
	        {
	            .image   = ft_get_swapchain_image( ft.swapchain, ft.image_index ),
	            .load_op = FT_ATTACHMENT_LOAD_OP_CLEAR,
	        },
	    .depth_attachment =
	        {
	            .image   = ft.depth_image,
	            .load_op = FT_ATTACHMENT_LOAD_OP_CLEAR,
	            .clear_value =
	                {
	                    .depth_stencil =
	                        {
	                            .depth   = 1.0f,
	                            .stencil = 0,
	                        },
	                },
	        },
	};

	ft_cmd_begin_render_pass( ft.cmd, &rpbi );
}

void FT_EndFrame( void )
{
	ft_cmd_end_render_pass( ft.cmd );

	struct ft_image_barrier barrier = {
	    .image     = ft_get_swapchain_image( ft.swapchain, ft.image_index ),
	    .old_state = FT_RESOURCE_STATE_COLOR_ATTACHMENT,
	    .new_state = FT_RESOURCE_STATE_PRESENT,
	};

	ft_cmd_barrier( ft.cmd, 0, NULL, 0, NULL, 1, &barrier );

	ft_end_command_buffer( ft.cmd );
	struct ft_queue_submit_info submit_info = {
	    .wait_semaphore_count   = 1,
	    .wait_semaphores        = &ft.frames[ft.frame_index].present_semaphore,
	    .command_buffer_count   = 1,
	    .command_buffers        = &ft.frames[ft.frame_index].cmd,
	    .signal_semaphore_count = 1,
	    .signal_semaphores      = &ft.frames[ft.frame_index].render_semaphore,
	    .signal_fence           = ft.frames[ft.frame_index].render_fence,
	};

	ft_queue_submit( ft.queue, &submit_info );

	struct ft_queue_present_info queue_present_info = {
	    .wait_semaphore_count = 1,
	    .wait_semaphores      = &ft.frames[ft.frame_index].render_semaphore,
	    .swapchain            = ft.swapchain,
	    .image_index          = ft.image_index,
	};

	ft_queue_present( ft.queue, &queue_present_info );

	ft.frames[ft.frame_index].cmd_recorded = 0;
	ft.frame_index                         = ( ft.frame_index + 1 ) % FRAME_COUNT;
}