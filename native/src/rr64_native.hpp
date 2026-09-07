#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned int rr64_lod_racer_view(unsigned char*, unsigned int, unsigned int);
void rr64_trace_guest_stage(const char* stage);
void rr64_lod_shadow_finish_hold(unsigned char* rdram, void* context);
unsigned int rr64_music_volume_update(unsigned int original);
unsigned int rr64_music_stock_volume(unsigned int original);

void rr64_lod_release_node(unsigned char* rdram, unsigned int node);
void rr64_lod_reset_actor_pool(unsigned char* rdram);
unsigned int rr64_combined_video_callback(unsigned char* rdram, unsigned int original);
void rr64_trace_guest_value(const char* stage, unsigned int value);
void rr64_engine_observe_frame(unsigned char* rdram);
void rr64_engine_capture_pre_update(unsigned char* rdram);
void rr64_engine_capture_post_update(unsigned char* rdram);
void rr64_engine_capture_dispatch_boundary(
    unsigned char* rdram,
    unsigned int boundary,
    unsigned int target);
void rr64_engine_capture_race_update_boundary(
    unsigned char* rdram,
    unsigned int boundary);
void rr64_engine_capture_dynamics_boundary(
    unsigned char* rdram,
    unsigned int function_id,
    unsigned int boundary,
    unsigned int actor_address);
void rr64_engine_capture_dynamics_auxiliary_boundary(
    unsigned char* rdram,
    unsigned int function_id,
    unsigned int boundary,
    unsigned int actor_address,
    unsigned int auxiliary_0,
    unsigned int auxiliary_1,
    unsigned int auxiliary_2,
    unsigned int auxiliary_3,
    unsigned int auxiliary_4);
void rr64_engine_capture_dynamics_auxiliary8_boundary(
    unsigned char* rdram,
    unsigned int function_id,
    unsigned int boundary,
    unsigned int actor_address,
    unsigned int auxiliary_0,
    unsigned int auxiliary_1,
    unsigned int auxiliary_2,
    unsigned int auxiliary_3,
    unsigned int auxiliary_4,
    unsigned int auxiliary_5,
    unsigned int auxiliary_6,
    unsigned int auxiliary_7);
void rr64_engine_capture_actor_presentation(unsigned char* rdram, unsigned int viewport);
void rr64_set_high_detail_actors_enabled(int enabled);
int rr64_is_high_detail_actors_enabled();
// Isolated R1 presentation candidate. These hooks never invoke the legacy
// real-RDRAM actor graph transaction. All preparation executes in a clone.
int rr64_render_only_max_lod_enabled();
void rr64_lod_read_stats(unsigned long long* preparations, unsigned long long* published_pairs,
    unsigned long long* consumed_actors, unsigned long long* fallback_actors);
void rr64_lod_observe_allocation(unsigned char* rdram, unsigned int node,
    unsigned int viewport, unsigned int bytes);
void rr64_lod_invalidate(unsigned char* rdram);
void rr64_lod_begin_preparation(unsigned char* rdram);
void rr64_lod_observe_pair(unsigned char* rdram, void* context);
void rr64_lod_prepare_shadow(unsigned char* rdram, void* context, int direct_order);
int rr64_lod_shadow_rider(unsigned char* rdram, unsigned int node);
void rr64_lod_shadow_stage(unsigned char* rdram, unsigned int node, unsigned int stage);
void rr64_lod_shadow_full_weight(unsigned char* rdram, void* context, int completed);
void rr64_lod_begin_draw(unsigned char* rdram);
void rr64_lod_end_draw(unsigned char* rdram);
unsigned int rr64_lod_select(unsigned char* rdram, unsigned int node, unsigned int stock_lod);
void rr64_lod_observe_rider_range(unsigned char* rdram, unsigned int node, int in_range);
unsigned int rr64_lod_actor_hidden(unsigned char* rdram, unsigned int node, unsigned int original_hidden);
unsigned int rr64_lod_root_source(unsigned char* rdram, unsigned int node,
    unsigned int record, unsigned int original_source);
void rr64_lod_scale_root_matrix(unsigned char* rdram, unsigned int node,
    unsigned int record, unsigned int matrix_address);
void rr64_lod_end_actor();
int rr64_world_distance_enabled();
void rr64_world_invalidate(unsigned char* rdram);
void rr64_world_observe_allocation(unsigned char* rdram, unsigned node, unsigned view, unsigned bytes);
void rr64_world_observe_roots(unsigned char* rdram, unsigned type);
void rr64_world_begin_draw(unsigned char* rdram);
void rr64_world_end_draw(unsigned char* rdram);
unsigned rr64_world_actor_hidden(unsigned char* rdram, unsigned node, unsigned hidden, const void* context);
unsigned rr64_world_select(unsigned char* rdram, unsigned node, unsigned stock_lod);
unsigned rr64_world_root_source(unsigned char* rdram, unsigned node, unsigned record, unsigned source);
void rr64_world_scale_root_matrix(unsigned char* rdram, unsigned node, unsigned record, unsigned matrix);
void rr64_world_end_actor();
void rr64_world_camera_far(unsigned char* rdram, void* context);
void rr64_world_camera_normalization(unsigned char* rdram, void* context);
void rr64_world_terrain_begin(unsigned char* rdram);
unsigned rr64_world_terrain_stock_state(unsigned char* rdram,unsigned record,unsigned state);
void rr64_world_terrain_observe(unsigned char* rdram, unsigned record);
void rr64_world_terrain_draw(unsigned char* rdram);
void rr64_world_objects_begin(unsigned char* rdram);
void rr64_world_objects_observe(unsigned char* rdram, unsigned placement);
void rr64_world_objects_sample(unsigned char* rdram, unsigned placement, unsigned graph);
void rr64_world_objects_draw(unsigned char* rdram);
void rr64_actor_begin_presentation_scope(unsigned char* rdram);
void rr64_actor_end_presentation_scope(unsigned char* rdram);
void rr64_actor_begin_presentation_pair(
    unsigned char* rdram,
    unsigned int bike_node,
    unsigned int rider_node);
int rr64_actor_presentation_transaction_active(
    unsigned char* rdram,
    unsigned int node);
void rr64_actor_trace_render_list(
    unsigned char* rdram,
    unsigned int list_head,
    unsigned int renderer);
void rr64_actor_trace_pose_bindings(
    unsigned char* rdram,
    unsigned int stage);
unsigned int rr64_actor_select_render_lod(
    unsigned char* rdram,
    unsigned int node,
    unsigned int selected_lod);
int rr64_actor_select_render_pose_source(
    unsigned char* rdram,
    unsigned int node,
    unsigned int selected_lod,
    unsigned int model_record,
    unsigned int original_pose_source);
void rr64_actor_trace_rendered_matrix(
    unsigned char* rdram,
    unsigned int node,
    unsigned int selected_lod,
    unsigned int model_record,
    unsigned int source_matrix,
    unsigned int rendered_matrix,
    unsigned int transform_buffer,
    unsigned int render_flag);
void rr64_actor_restore_presentation_transactions(unsigned char* rdram);
int rr64_engine_contract_has_warning();
void rr64_trace_race_frame(
    unsigned char* rdram,
    void* context,
    unsigned int mode,
    unsigned int pending_mode,
    unsigned int pause_state,
    unsigned int physics_delta_bits,
    unsigned int update_ticks_bits,
    unsigned int wait_ticks_bits,
    unsigned int total_ticks_bits);
int rr64_is_live_race_mode(unsigned int mode);
int rr64_is_race_mode_active();
int rr64_is_gameplay_feedback_active();
int rr64_are_gameplay_shortcuts_active();
int rr64_is_road_rumble_allowed();
void rr64_set_rumble_enabled(int enabled);
int rr64_is_rumble_enabled();
void rr64_request_rider_eject();
int rr64_local_rider_has_fists_selected();
unsigned int rr64_audio_timeline_epoch();
void rr64_set_maximum_view_distance_enabled(int enabled);
int rr64_is_maximum_view_distance_enabled();
unsigned int rr64_maximum_view_distance_map_range(unsigned int original_bits);
unsigned int rr64_traffic_render_visibility(
    unsigned char* rdram,
    unsigned int node,
    unsigned int stock_hidden);
unsigned int rr64_terrain_unload_decision(
    unsigned char* rdram,
    unsigned int cell,
    unsigned int stock_unload);
void rr64_render_resident_terrain(unsigned char* rdram, void* context);
void rr64_trace_terrain_scene(unsigned char* rdram);
void rr64_trace_lod_node(unsigned char* rdram, unsigned int kind, unsigned int node);
unsigned int rr64_online_menu_route_mode(unsigned int requested_mode);
void rr64_online_menu_apply_pending_guest_input(unsigned char* rdram);
void rr64_online_game_setup_before_update(unsigned char* rdram);
void rr64_online_game_setup_after_update(unsigned char* rdram);
unsigned int rr64_online_requested_racer_count(unsigned int original_count);
unsigned int rr64_online_prepare_render_layout(unsigned int stock_layout);
unsigned int rr64_online_render_first_viewport(unsigned int stock_viewport);
unsigned int rr64_online_render_geometry_viewport(unsigned int stock_viewport);
void rr64_online_restore_active_viewport(
    unsigned char* rdram,
    unsigned int stock_viewport);
void rr64_online_race_sync_before_update(unsigned char* rdram, unsigned int mode);
void rr64_online_race_sync_after_update(unsigned char* rdram, unsigned int mode);
void rr64_trace_guest_input(
    unsigned int mode,
    unsigned int active_mask,
    unsigned int buttons,
    unsigned int stick_x_byte,
    unsigned int stick_y_byte,
    unsigned int error_0,
    unsigned int error_1,
    unsigned int error_2,
    unsigned int error_3);
void rr64_autotest_input(unsigned char* rdram, unsigned int mode);
void rr64_combat_impact_rumble(
    unsigned char* rdram,
    unsigned int first_bike,
    unsigned int second_bike,
    unsigned int strength_percent);
enum {
    RR64_PROMPT_BUTTON_A = 0,
    RR64_PROMPT_BUTTON_B = 1,
};
unsigned int rr64_write_button_prompt(
    unsigned char* rdram,
    unsigned int logical_button,
    unsigned int destination,
    unsigned int capacity);
void rr64_name_entry_navigation(
    unsigned char* rdram,
    unsigned int column_count,
    unsigned int maximum_row);
void rr64_online_apply_display_names(unsigned char* rdram);
void rr64_achievement_observe_frame(unsigned char* rdram);
void rr64_achievement_game_event(
    unsigned char* rdram,
    unsigned int event_id,
    unsigned int value,
    unsigned int actor);
void rr64_achievement_campaign_level_advanced(
    unsigned char* rdram,
    unsigned int new_level);
void rr64_achievement_campaign_completed(unsigned char* rdram);
#ifdef __cplusplus
}
#endif

void rr64_register_overlays();




