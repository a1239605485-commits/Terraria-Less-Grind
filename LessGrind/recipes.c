#include <stddef.h>
#include "mod_logger.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"
#include "tefkernel/patchlib/struct/array.h"

/* Recipe.SetupRecipes runs after every world/game-data refresh.  We alter the
 * completed vanilla recipe objects rather than manufacture items or alter combat. */
static patch_handle_t g_main_recipes = PATCH_NULL;
static patch_handle_t g_recipe_create = PATCH_NULL;
static patch_handle_t g_recipe_required = PATCH_NULL;
static patch_handle_t g_item_type = PATCH_NULL;
static patch_handle_t g_item_stack = PATCH_NULL;
static patch_hook_id_t g_hook = PATCH_HOOK_INVALID_ID;
static int g_applied = 0;

static int contains(const int* values, size_t count, int value) {
    for (size_t i = 0; i < count; ++i) if (values[i] == value) return 1;
    return 0;
}

static void halve_ingredients(patch_handle_t recipe) {
    patch_handle_t items = PATCH_NULL;
    patchlib_field_get_value(g_recipe_required, recipe, &items);
    if (!items) return;
    size_t length = patchlib_array_length(items);
    for (size_t i = 0; i < length; ++i) {
        patch_handle_t ingredient = PATCH_NULL;
        if (patchlib_array_at(items, i, &ingredient) != 0 || !ingredient) continue;
        int stack = 0;
        patchlib_field_get_value(g_item_stack, ingredient, &stack);
        if (stack > 1) {
            int cheaper = (stack + 1) / 2; /* round up: no free ingredients */
            patchlib_field_set_value(g_item_stack, ingredient, &cheaper);
        }
    }
}

static void apply_recipe_changes(void) {
    if (g_applied) return;
    patch_handle_t recipes = PATCH_NULL;
    patchlib_field_get_value(g_main_recipes, NULL, &recipes);
    if (!recipes) {
        mod_logger_write(MOD_LOG_LEVEL_WARNING, "LessGrind", "Main.recipe is unavailable");
        return;
    }

    /* Vanilla item IDs: basic blocks/walls, frequently brewed potions, and Boss summons. */
    static const int building_items[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        22, 25, 26, 27, 28, 29, 30, 31, 32, 38, 39, 40, 41, 42, 94, 95, 96
    };
    static const int common_potions[] = {
        126, 227, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298,
        299, 300, 301, 302, 303, 304, 305, 306, 2322, 2323
    };
    static const int boss_summons[] = { 43, 267, 544, 556, 557, 560, 1133, 1293, 1330 };

    size_t total = patchlib_array_length(recipes);
    int building = 0, potions = 0, summons = 0;
    for (size_t i = 0; i < total; ++i) {
        patch_handle_t recipe = PATCH_NULL, result = PATCH_NULL;
        if (patchlib_array_at(recipes, i, &recipe) != 0 || !recipe) continue;
        patchlib_field_get_value(g_recipe_create, recipe, &result);
        if (!result) continue;
        int type = 0, stack = 0;
        patchlib_field_get_value(g_item_type, result, &type);
        patchlib_field_get_value(g_item_stack, result, &stack);
        if (contains(building_items, sizeof(building_items)/sizeof(building_items[0]), type)) {
            int amount = stack > 0 ? stack * 3 : 3;
            patchlib_field_set_value(g_item_stack, result, &amount);
            ++building;
        } else if (contains(common_potions, sizeof(common_potions)/sizeof(common_potions[0]), type)) {
            int amount = stack > 0 ? stack * 5 : 5;
            patchlib_field_set_value(g_item_stack, result, &amount);
            ++potions;
        } else if (contains(boss_summons, sizeof(boss_summons)/sizeof(boss_summons[0]), type)) {
            halve_ingredients(recipe);
            ++summons;
        }
    }
    g_applied = 1;
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind",
        "Recipes updated: building=%d, potion=%d, Boss summon=%d", building, potions, summons);
}

static void on_setup_recipes(patch_handle_t instance, void** args, void* result,
                             const patch_method_signature_t* sig) {
    (void)instance; (void)args; (void)result; (void)sig;
    apply_recipe_changes();
}

void less_grind_recipes_init(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t recipe_type = patchlib_type_get_type("Terraria", "Recipe");
    patch_handle_t item_type = patchlib_type_get_type("Terraria", "Item");
    if (!main_type || !recipe_type || !item_type) goto done;
    g_main_recipes = patchlib_type_get_field(main_type, "recipe");
    g_recipe_create = patchlib_type_get_field(recipe_type, "createItem");
    g_recipe_required = patchlib_type_get_field(recipe_type, "requiredItem");
    g_item_type = patchlib_type_get_field(item_type, "type");
    g_item_stack = patchlib_type_get_field(item_type, "stack");
    patch_handle_t setup = patchlib_type_get_method_by_param_count(recipe_type, "SetupRecipes", 0);
    if (g_main_recipes && g_recipe_create && g_recipe_required && g_item_type && g_item_stack && setup) {
        g_hook = patchlib_install_prepost_hook(setup, NULL, on_setup_recipes);
    }
    if (setup) patchlib_free(setup);
done:
    if (main_type) patchlib_free(main_type);
    if (recipe_type) patchlib_free(recipe_type);
    if (item_type) patchlib_free(item_type);
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind", "Recipe hook: %s",
        g_hook == PATCH_HOOK_INVALID_ID ? "failed" : "ready");
}

void less_grind_recipes_cleanup(void) {
    if (g_hook != PATCH_HOOK_INVALID_ID) patchlib_uninstall_hook(g_hook);
    if (g_main_recipes) patchlib_free(g_main_recipes);
    if (g_recipe_create) patchlib_free(g_recipe_create);
    if (g_recipe_required) patchlib_free(g_recipe_required);
    if (g_item_type) patchlib_free(g_item_type);
    if (g_item_stack) patchlib_free(g_item_stack);
    g_hook = PATCH_HOOK_INVALID_ID; g_applied = 0;
}
