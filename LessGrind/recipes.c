#include <stddef.h>
#include <stdbool.h>

#include "mod_logger.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"
#include "tefkernel/patchlib/struct/array.h"

/*
 * Terraria Less Grind v1.1.0
 * Recipe-side quality-of-life changes:
 *   1) Building blocks / platforms / walls craft x3
 *   2) Common useful potions craft x5
 *   3) Craftable boss summons use roughly half the ingredients (minimum 1)
 *
 * This file keeps the v1.0.4 TEFKernel bool-return fix and broadens the
 * gameplay coverage without changing the already-working load/hook structure.
 */

static patch_handle_t g_main_recipes = PATCH_NULL;
static patch_handle_t g_main_tile_solid = PATCH_NULL;

static patch_handle_t g_recipe_create = PATCH_NULL;
static patch_handle_t g_recipe_required = PATCH_NULL;

static patch_handle_t g_item_type = PATCH_NULL;
static patch_handle_t g_item_stack = PATCH_NULL;
static patch_handle_t g_item_create_tile = PATCH_NULL;
static patch_handle_t g_item_create_wall = PATCH_NULL;

static patch_hook_id_t g_hook = PATCH_HOOK_INVALID_ID;

static int g_applied = 0;
static int g_wait_log_count = 0;

/* Terraria's platform Tile ID.  Platforms are not tileSolid, so include them. */
#define TERRARIA_TILE_PLATFORMS 19

static int contains(const int* values, size_t count, int value) {
    for (size_t i = 0; i < count; ++i) {
        if (values[i] == value) {
            return 1;
        }
    }
    return 0;
}

/*
 * Potions intentionally covered by the first full release.
 *
 * Recovery:
 *   Lesser/normal/greater/super healing or mana, Restoration.
 *
 * Classic buffs:
 *   Obsidian Skin through Gravitation (288..305).
 *
 * Fishing / utility / combat buffs:
 *   Mining, Heartreach, Calming, Builder, Titan, Flipper, Summoning,
 *   Dangersense, Ammo Reservation, Lifeforce, Endurance, Rage, Inferno,
 *   Wrath, Recall, Teleportation, Fishing, Sonar, Crate, Warmth,
 *   Wormhole, Luck potions, Potion of Return, Biome Sight.
 *
 * We deliberately do not include Bottled Water, Love Potion, Stink Potion,
 * or Gender Change Potion.
 */
static const int g_common_potions[] = {
    28,    /* Lesser Healing Potion */
    110,   /* Lesser Mana Potion */
    188,   /* Healing Potion */
    189,   /* Mana Potion */
    227,   /* Restoration Potion */
    499,   /* Greater Healing Potion */
    500,   /* Greater Mana Potion (normally bought; harmless if no recipe) */

    288, 289, 290, 291, 292, 293, 294, 295, 296,
    297, 298, 299, 300, 301, 302, 303, 304, 305,

    2209,  /* Super Mana Potion */

    2322,  /* Mining Potion */
    2323,  /* Heartreach Potion */
    2324,  /* Calming Potion */
    2325,  /* Builder Potion */
    2326,  /* Titan Potion */
    2327,  /* Flipper Potion */
    2328,  /* Summoning Potion */
    2329,  /* Dangersense Potion */
    2344,  /* Ammo Reservation Potion */
    2345,  /* Lifeforce Potion */
    2346,  /* Endurance Potion */
    2347,  /* Rage Potion */
    2348,  /* Inferno Potion */
    2349,  /* Wrath Potion */
    2350,  /* Recall Potion */
    2351,  /* Teleportation Potion */
    2354,  /* Fishing Potion */
    2355,  /* Sonar Potion */
    2356,  /* Crate Potion */
    2359,  /* Warmth Potion */

    2997,  /* Wormhole Potion */
    3544,  /* Super Healing Potion */

    4477,  /* Lesser Luck Potion */
    4478,  /* Luck Potion */
    4479,  /* Greater Luck Potion */

    4870,  /* Potion of Return */
    5211   /* Biome Sight Potion */
};

/*
 * Craftable boss-summoning items.
 *
 * Non-craftable summon methods (Guide Voodoo Doll, Truffle Worm,
 * Gelatin Crystal, Lihzahrd Power Cell, etc.) are intentionally excluded,
 * because there is no recipe cost to reduce.
 */
static const int g_boss_summons[] = {
    43,    /* Suspicious Looking Eye */
    70,    /* Worm Food */
    544,   /* Mechanical Eye */
    556,   /* Mechanical Worm */
    557,   /* Mechanical Skull */
    560,   /* Slime Crown */
    1133,  /* Abeemination */
    1331,  /* Bloody Spine */
    3601,  /* Celestial Sigil */
    5120   /* Deer Thing */
};

/*
 * Return non-zero when the recipe result is a bulk building material.
 *
 * Instead of maintaining hundreds of item IDs, use Terraria's own runtime
 * placement metadata:
 *   - any wall item (createWall >= 0)
 *   - any solid tile item (Main.tileSolid[createTile])
 *   - platforms (Tile ID 19), which are not tileSolid
 *
 * This excludes most furniture / crafting stations / chests / decorations,
 * while naturally covering old and newly-added 1.4.5 blocks and walls.
 */
static int is_building_material(patch_handle_t item, patch_handle_t tile_solid) {
    if (!item || !g_item_create_tile || !g_item_create_wall) {
        return 0;
    }

    int create_tile = -1;
    int create_wall = -1;

    patchlib_field_get_value(g_item_create_tile, item, &create_tile);
    patchlib_field_get_value(g_item_create_wall, item, &create_wall);

    if (create_wall >= 0) {
        return 1;
    }

    if (create_tile < 0) {
        return 0;
    }

    if (create_tile == TERRARIA_TILE_PLATFORMS) {
        return 1;
    }

    if (!tile_solid) {
        return 0;
    }

    size_t tile_count = patchlib_array_length(tile_solid);
    if ((size_t)create_tile >= tile_count) {
        return 0;
    }

    bool solid = false;
    if (!patchlib_array_at(tile_solid, (size_t)create_tile, &solid)) {
        return 0;
    }

    return solid ? 1 : 0;
}

/*
 * Halve every ingredient stack, rounding upward and never reducing 1 to 0.
 * Returns how many ingredient entries were actually changed.
 */
static int halve_ingredients(patch_handle_t recipe) {
    patch_handle_t items = PATCH_NULL;
    patchlib_field_get_value(g_recipe_required, recipe, &items);

    if (!items) {
        return 0;
    }

    size_t length = patchlib_array_length(items);
    int changed = 0;

    for (size_t i = 0; i < length; ++i) {
        patch_handle_t ingredient = PATCH_NULL;

        /* TEFKernel: true = array read succeeded. */
        if (!patchlib_array_at(items, i, &ingredient) || !ingredient) {
            continue;
        }

        int stack = 0;
        patchlib_field_get_value(g_item_stack, ingredient, &stack);

        if (stack > 1) {
            int cheaper = (stack + 1) / 2;
            patchlib_field_set_value(g_item_stack, ingredient, &cheaper);
            ++changed;
        }
    }

    return changed;
}

static void apply_recipe_changes(void) {
    if (g_applied) {
        return;
    }

    patch_handle_t recipes = PATCH_NULL;
    patchlib_field_get_value(g_main_recipes, NULL, &recipes);

    if (!recipes) {
        if (g_wait_log_count < 3) {
            mod_logger_write(
                MOD_LOG_LEVEL_WARNING,
                "LessGrind",
                "Recipe table not ready yet: Terraria.Main.recipe is NULL"
            );
            ++g_wait_log_count;
        }
        return;
    }

    size_t total = patchlib_array_length(recipes);
    if (total == 0) {
        if (g_wait_log_count < 3) {
            mod_logger_write(
                MOD_LOG_LEVEL_WARNING,
                "LessGrind",
                "Recipe table exists but length is 0; will retry"
            );
            ++g_wait_log_count;
        }
        return;
    }

    /* Read Main.tileSolid once for the whole scan. */
    patch_handle_t tile_solid = PATCH_NULL;
    if (g_main_tile_solid) {
        patchlib_field_get_value(g_main_tile_solid, NULL, &tile_solid);
    }

    int building = 0;
    int potions = 0;
    int summons = 0;
    int summon_ingredients = 0;

    size_t read_ok = 0;
    size_t read_failed = 0;

    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",
        "Recipe scan started: total=%zu, tileSolid=%p",
        total,
        tile_solid
    );

    for (size_t i = 0; i < total; ++i) {
        patch_handle_t recipe = PATCH_NULL;
        patch_handle_t result = PATCH_NULL;

        if (!patchlib_array_at(recipes, i, &recipe) || !recipe) {
            ++read_failed;
            continue;
        }
        ++read_ok;

        patchlib_field_get_value(g_recipe_create, recipe, &result);
        if (!result) {
            continue;
        }

        int type = 0;
        int stack = 0;

        patchlib_field_get_value(g_item_type, result, &type);
        patchlib_field_get_value(g_item_stack, result, &stack);

        /* Boss summon cost takes priority over output multipliers. */
        if (contains(
                g_boss_summons,
                sizeof(g_boss_summons) / sizeof(g_boss_summons[0]),
                type
            )) {

            int changed = halve_ingredients(recipe);
            summon_ingredients += changed;
            ++summons;

            mod_logger_write(
                MOD_LOG_LEVEL_DEBUG,
                "LessGrind",
                "Boss summon recipe changed: item=%d, ingredients_changed=%d",
                type,
                changed
            );
        }

        /* Explicit potion list comes before placeable/building detection. */
        else if (contains(
                    g_common_potions,
                    sizeof(g_common_potions) / sizeof(g_common_potions[0]),
                    type
                 )) {

            int amount = stack > 0 ? stack * 5 : 5;
            patchlib_field_set_value(g_item_stack, result, &amount);
            ++potions;

            mod_logger_write(
                MOD_LOG_LEVEL_DEBUG,
                "LessGrind",
                "Potion recipe changed: item=%d, stack=%d->%d",
                type,
                stack,
                amount
            );
        }

        else if (is_building_material(result, tile_solid)) {
            int amount = stack > 0 ? stack * 3 : 3;
            patchlib_field_set_value(g_item_stack, result, &amount);
            ++building;

            mod_logger_write(
                MOD_LOG_LEVEL_DEBUG,
                "LessGrind",
                "Building recipe changed: item=%d, stack=%d->%d",
                type,
                stack,
                amount
            );
        }
    }

    /* Only lock after proving that the recipe array was actually readable. */
    if (read_ok > 0) {
        g_applied = 1;
    }

    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",
        "Recipe scan finished: total=%zu, read_ok=%zu, read_failed=%zu, "
        "building=%d, potion=%d, boss_summon=%d, summon_ingredients=%d, applied=%d",
        total,
        read_ok,
        read_failed,
        building,
        potions,
        summons,
        summon_ingredients,
        g_applied
    );

    if (read_ok > 0 && !tile_solid) {
        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",
            "Recipe scan could not read Main.tileSolid; potion and boss-summon "
            "features still work, but automatic building-material detection is disabled"
        );
    }
}

static void on_main_update(
    patch_handle_t instance,
    void** args,
    void* result,
    const patch_method_signature_t* sig
) {
    (void)instance;
    (void)args;
    (void)result;
    (void)sig;

    apply_recipe_changes();
}

void less_grind_recipes_init(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t recipe_type = patchlib_type_get_type("Terraria", "Recipe");
    patch_handle_t item_type = patchlib_type_get_type("Terraria", "Item");

    if (!main_type || !recipe_type || !item_type) {
        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",
            "Recipe init failed: required Terraria type not found "
            "(Main=%p Recipe=%p Item=%p)",
            main_type,
            recipe_type,
            item_type
        );
        goto done;
    }

    g_main_recipes = patchlib_type_get_field(main_type, "recipe");
    g_main_tile_solid = patchlib_type_get_field(main_type, "tileSolid");

    g_recipe_create = patchlib_type_get_field(recipe_type, "createItem");
    g_recipe_required = patchlib_type_get_field(recipe_type, "requiredItem");

    g_item_type = patchlib_type_get_field(item_type, "type");
    g_item_stack = patchlib_type_get_field(item_type, "stack");
    g_item_create_tile = patchlib_type_get_field(item_type, "createTile");
    g_item_create_wall = patchlib_type_get_field(item_type, "createWall");

    patch_handle_t update =
        patchlib_type_get_method_by_param_count(main_type, "Update", 1);

    /* Core handles required by all recipe features. */
    if (!g_main_recipes || !g_recipe_create || !g_recipe_required ||
        !g_item_type || !g_item_stack || !update) {

        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",
            "Recipe init missing core handle: recipe=%p createItem=%p requiredItem=%p "
            "item.type=%p item.stack=%p Update=%p",
            g_main_recipes,
            g_recipe_create,
            g_recipe_required,
            g_item_type,
            g_item_stack,
            update
        );
    }
    else {
        g_hook = patchlib_install_prepost_hook(update, NULL, on_main_update);
    }

    /* Building-only metadata is optional so potion/boss logic remains usable. */
    if (!g_main_tile_solid || !g_item_create_tile || !g_item_create_wall) {
        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",
            "Building metadata incomplete: tileSolid=%p createTile=%p createWall=%p; "
            "automatic building-material detection may be unavailable",
            g_main_tile_solid,
            g_item_create_tile,
            g_item_create_wall
        );
    }

    if (update) {
        patchlib_free(update);
    }

done:
    if (main_type) {
        patchlib_free(main_type);
    }
    if (recipe_type) {
        patchlib_free(recipe_type);
    }
    if (item_type) {
        patchlib_free(item_type);
    }

    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",
        "Recipe update hook: %s (id=%d)",
        g_hook == PATCH_HOOK_INVALID_ID ? "failed" : "ready",
        (int)g_hook
    );
}

void less_grind_recipes_cleanup(void) {
    if (g_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_hook);
    }

    if (g_main_recipes) {
        patchlib_free(g_main_recipes);
    }
    if (g_main_tile_solid) {
        patchlib_free(g_main_tile_solid);
    }
    if (g_recipe_create) {
        patchlib_free(g_recipe_create);
    }
    if (g_recipe_required) {
        patchlib_free(g_recipe_required);
    }
    if (g_item_type) {
        patchlib_free(g_item_type);
    }
    if (g_item_stack) {
        patchlib_free(g_item_stack);
    }
    if (g_item_create_tile) {
        patchlib_free(g_item_create_tile);
    }
    if (g_item_create_wall) {
        patchlib_free(g_item_create_wall);
    }

    g_main_recipes = PATCH_NULL;
    g_main_tile_solid = PATCH_NULL;
    g_recipe_create = PATCH_NULL;
    g_recipe_required = PATCH_NULL;
    g_item_type = PATCH_NULL;
    g_item_stack = PATCH_NULL;
    g_item_create_tile = PATCH_NULL;
    g_item_create_wall = PATCH_NULL;

    g_hook = PATCH_HOOK_INVALID_ID;
    g_applied = 0;
    g_wait_log_count = 0;
}
