/* Inspect live state for the three upstream serializers that malloc buffers
 * with unwritten padding; hash all other serialized state verbatim. Neither
 * production allocation nor production serialization is changed for this test. */
#include "building/building.h"
#include "figure/figure.h"
#include "game/file_io.c"
#include "planning/sha256.h"
#include "scenario/data.h"
static void snapshot_buffer(plan_sha256* s, const char* name, const buffer* b)
{
    if (!b)
        return;
    plan_sha256_update(s, name, strlen(name) + 1);
    size_t length
        = (!strcmp(name, "scenario_formulas") || !strcmp(name, "scenario_texts")) ? b->index : b->size;
    plan_sha256_update(s, &length, sizeof(length));
    plan_sha256_update(s, b->data, length);
}
void planning_test_snapshot(uint8_t digest[32])
{
    resource_set_mapping(RESOURCE_CURRENT_VERSION);
    init_savegame_data(SAVE_GAME_CURRENT_VERSION);
    savegame_save_to_state(&savegame_data.state);
    plan_sha256 s;
    plan_sha256_init(&s);
    snapshot_buffer(&s, "resource_version", savegame_data.state.resource_version);
    snapshot_buffer(&s, "scenario_campaign_mission", savegame_data.state.scenario_campaign_mission);
    snapshot_buffer(&s, "file_version", savegame_data.state.file_version);
    snapshot_buffer(&s, "scenario_version", savegame_data.state.scenario_version);
    snapshot_buffer(&s, "image_grid", savegame_data.state.image_grid);
    snapshot_buffer(&s, "edge_grid", savegame_data.state.edge_grid);
    snapshot_buffer(&s, "building_grid", savegame_data.state.building_grid);
    snapshot_buffer(&s, "terrain_grid", savegame_data.state.terrain_grid);
    snapshot_buffer(&s, "aqueduct_grid", savegame_data.state.aqueduct_grid);
    snapshot_buffer(&s, "figure_grid", savegame_data.state.figure_grid);
    snapshot_buffer(&s, "bitfields_grid", savegame_data.state.bitfields_grid);
    snapshot_buffer(&s, "sprite_grid", savegame_data.state.sprite_grid);
    snapshot_buffer(&s, "random_grid", savegame_data.state.random_grid);
    snapshot_buffer(&s, "desirability_grid", savegame_data.state.desirability_grid);
    snapshot_buffer(&s, "elevation_grid", savegame_data.state.elevation_grid);
    snapshot_buffer(&s, "building_damage_grid", savegame_data.state.building_damage_grid);
    snapshot_buffer(&s, "aqueduct_backup_grid", savegame_data.state.aqueduct_backup_grid);
    snapshot_buffer(&s, "sprite_backup_grid", savegame_data.state.sprite_backup_grid);
    snapshot_buffer(&s, "route_figures", savegame_data.state.route_figures);
    snapshot_buffer(&s, "route_paths", savegame_data.state.route_paths);
    snapshot_buffer(&s, "formations", savegame_data.state.formations);
    snapshot_buffer(&s, "formation_totals", savegame_data.state.formation_totals);
    snapshot_buffer(&s, "city_data", savegame_data.state.city_data);
    snapshot_buffer(&s, "city_faction_unknown", savegame_data.state.city_faction_unknown);
    snapshot_buffer(&s, "player_name", savegame_data.state.player_name);
    snapshot_buffer(&s, "city_faction", savegame_data.state.city_faction);
    snapshot_buffer(&s, "city_view_orientation", savegame_data.state.city_view_orientation);
    snapshot_buffer(&s, "game_time", savegame_data.state.game_time);
    snapshot_buffer(&s, "building_extra_highest_id_ever", savegame_data.state.building_extra_highest_id_ever);
    snapshot_buffer(&s, "random_iv", savegame_data.state.random_iv);
    snapshot_buffer(&s, "city_view_camera", savegame_data.state.city_view_camera);
    snapshot_buffer(&s, "building_count_culture1", savegame_data.state.building_count_culture1);
    snapshot_buffer(&s, "city_graph_order", savegame_data.state.city_graph_order);
    snapshot_buffer(&s, "emperor_change_time", savegame_data.state.emperor_change_time);
    snapshot_buffer(&s, "empire", savegame_data.state.empire);
    snapshot_buffer(&s, "empire_map", savegame_data.state.empire_map);
    snapshot_buffer(&s, "empire_cities", savegame_data.state.empire_cities);
    snapshot_buffer(&s, "building_count_industry", savegame_data.state.building_count_industry);
    snapshot_buffer(&s, "trade_prices", savegame_data.state.trade_prices);
    snapshot_buffer(&s, "figure_names", savegame_data.state.figure_names);
    snapshot_buffer(&s, "culture_coverage", savegame_data.state.culture_coverage);
    snapshot_buffer(&s, "scenario_events", savegame_data.state.scenario_events);
    snapshot_buffer(&s, "scenario_conditions", savegame_data.state.scenario_conditions);
    snapshot_buffer(&s, "scenario_actions", savegame_data.state.scenario_actions);
    snapshot_buffer(&s, "scenario_formulas", savegame_data.state.scenario_formulas);
    snapshot_buffer(&s, "scenario_texts", savegame_data.state.scenario_texts);
    snapshot_buffer(&s, "custom_messages", savegame_data.state.custom_messages);
    snapshot_buffer(&s, "custom_media", savegame_data.state.custom_media);
    snapshot_buffer(&s, "requests", savegame_data.state.requests);
    snapshot_buffer(&s, "invasions", savegame_data.state.invasions);
    snapshot_buffer(&s, "demand_changes", savegame_data.state.demand_changes);
    snapshot_buffer(&s, "price_changes", savegame_data.state.price_changes);
    snapshot_buffer(&s, "allowed_buildings", savegame_data.state.allowed_buildings);
    snapshot_buffer(&s, "custom_variables", savegame_data.state.custom_variables);
    snapshot_buffer(&s, "message_media_text_blob", savegame_data.state.message_media_text_blob);
    snapshot_buffer(&s, "message_media_metadata", savegame_data.state.message_media_metadata);
    snapshot_buffer(&s, "max_game_year", savegame_data.state.max_game_year);
    snapshot_buffer(&s, "earthquake", savegame_data.state.earthquake);
    snapshot_buffer(&s, "emperor_change_state", savegame_data.state.emperor_change_state);
    snapshot_buffer(&s, "messages", savegame_data.state.messages);
    snapshot_buffer(&s, "message_extra", savegame_data.state.message_extra);
    snapshot_buffer(&s, "population_messages", savegame_data.state.population_messages);
    snapshot_buffer(&s, "message_counts", savegame_data.state.message_counts);
    snapshot_buffer(&s, "message_delays", savegame_data.state.message_delays);
    snapshot_buffer(&s, "building_list_burning_totals", savegame_data.state.building_list_burning_totals);
    snapshot_buffer(&s, "figure_sequence", savegame_data.state.figure_sequence);
    snapshot_buffer(&s, "scenario_settings", savegame_data.state.scenario_settings);
    snapshot_buffer(&s, "invasion_warnings", savegame_data.state.invasion_warnings);
    snapshot_buffer(&s, "scenario_is_custom", savegame_data.state.scenario_is_custom);
    snapshot_buffer(&s, "city_sounds", savegame_data.state.city_sounds);
    snapshot_buffer(&s, "building_extra_highest_id", savegame_data.state.building_extra_highest_id);
    snapshot_buffer(&s, "figure_traders", savegame_data.state.figure_traders);
    snapshot_buffer(&s, "building_list_burning", savegame_data.state.building_list_burning);
    snapshot_buffer(&s, "building_list_small", savegame_data.state.building_list_small);
    snapshot_buffer(&s, "building_list_large", savegame_data.state.building_list_large);
    snapshot_buffer(&s, "tutorial_part1", savegame_data.state.tutorial_part1);
    snapshot_buffer(&s, "building_count_military", savegame_data.state.building_count_military);
    snapshot_buffer(&s, "enemy_army_totals", savegame_data.state.enemy_army_totals);
    snapshot_buffer(&s, "building_storages", savegame_data.state.building_storages);
    snapshot_buffer(&s, "building_count_culture2", savegame_data.state.building_count_culture2);
    snapshot_buffer(&s, "building_count_support", savegame_data.state.building_count_support);
    snapshot_buffer(&s, "tutorial_part2", savegame_data.state.tutorial_part2);
    snapshot_buffer(&s, "gladiator_revolt", savegame_data.state.gladiator_revolt);
    snapshot_buffer(&s, "trade_route_limit", savegame_data.state.trade_route_limit);
    snapshot_buffer(&s, "trade_route_traded", savegame_data.state.trade_route_traded);
    snapshot_buffer(&s, "trade_routes", savegame_data.state.trade_routes);
    snapshot_buffer(&s, "trade_history", savegame_data.state.trade_history);
    snapshot_buffer(&s, "building_barracks_tower_sentry", savegame_data.state.building_barracks_tower_sentry);
    snapshot_buffer(&s, "building_extra_sequence", savegame_data.state.building_extra_sequence);
    snapshot_buffer(&s, "routing_counters", savegame_data.state.routing_counters);
    snapshot_buffer(&s, "building_count_culture3", savegame_data.state.building_count_culture3);
    snapshot_buffer(&s, "enemy_armies", savegame_data.state.enemy_armies);
    snapshot_buffer(&s, "city_entry_exit_xy", savegame_data.state.city_entry_exit_xy);
    snapshot_buffer(&s, "last_invasion_id", savegame_data.state.last_invasion_id);
    snapshot_buffer(&s, "building_extra_corrupt_houses", savegame_data.state.building_extra_corrupt_houses);
    snapshot_buffer(&s, "scenario_name", savegame_data.state.scenario_name);
    snapshot_buffer(&s, "bookmarks", savegame_data.state.bookmarks);
    snapshot_buffer(&s, "tutorial_part3", savegame_data.state.tutorial_part3);
    snapshot_buffer(&s, "city_entry_exit_grid_offset", savegame_data.state.city_entry_exit_grid_offset);
    snapshot_buffer(&s, "campaign_name", savegame_data.state.campaign_name);
    snapshot_buffer(&s, "end_marker", savegame_data.state.end_marker);
    snapshot_buffer(&s, "deliveries", savegame_data.state.deliveries);
    snapshot_buffer(&s, "custom_empire", savegame_data.state.custom_empire);
    snapshot_buffer(&s, "visited_buildings", savegame_data.state.visited_buildings);
    snapshot_buffer(&s, "model_data", savegame_data.state.model_data);
    snapshot_buffer(&s, "rubble_grid", savegame_data.state.rubble_grid);
    snapshot_buffer(&s, "production_rates", savegame_data.state.production_rates);
    snapshot_buffer(&s, "monument_stages", savegame_data.state.monument_stages);
    snapshot_buffer(&s, "finance_ledger", savegame_data.state.finance_ledger);
    int buildings = building_count();
    plan_sha256_update(&s, &buildings, sizeof(buildings));
    for (int i = 0; i < buildings; ++i)
        plan_sha256_update(&s, building_get(i), sizeof(building));
    unsigned figures = figure_count();
    plan_sha256_update(&s, &figures, sizeof(figures));
    for (unsigned i = 0; i < figures; ++i)
        plan_sha256_update(&s, figure_get(i), sizeof(figure));
    plan_sha256_update(&s, &scenario, sizeof(scenario));
    plan_sha256_finish(&s, digest);
    clear_savegame_pieces();
}
