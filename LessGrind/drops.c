#include <stddef.h>

#include "mod_logger.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"

/*
 * Terraria Less Grind v1.1.0
 * Guaranteed grind-material drops.
 *
 * This keeps the v1.0.4 bool-return fix:
 * patchlib_method_invoke_args(...) returns true on success.
 */

static patch_handle_t g_common = PATCH_NULL;
static patch_handle_t g_register = PATCH_NULL;
static patch_handle_t g_item_drops_db = PATCH_NULL;

static patch_hook_id_t g_hook = PATCH_HOOK_INVALID_ID;

/* One bit per rule; the table below intentionally stays below 32 entries. */
static unsigned int g_registered_mask = 0;
static int g_registered = 0;
static int g_database_wait_logs = 0;

typedef struct guaranteed_drop_t {
    int npc_id;
    int item_id;
    const char* name;
} guaranteed_drop_t;

/*
 * First full release coverage.
 *
 * These are materials whose normal farming can involve repeated kills.
 * The mod adds one guaranteed copy; vanilla drop rules remain intact, so an
 * enemy can occasionally drop its normal copy in addition to this guaranteed
 * one.
 */
static const guaranteed_drop_t g_drops[] = {
    { 6,   68,   "Eater of Souls -> Rotten Chunk" },
    { 173, 1330, "Crimera -> Vertebra" },
    { 2,   38,   "Demon Eye -> Lens" },
    { 42,  209,  "Hornet -> Stinger" },
    { 69,  323,  "Antlion -> Antlion Mandible" },
    { 48,  320,  "Harpy -> Feather" },
    { 65,  319,  "Shark -> Shark Fin" },
    { 153, 1328, "Giant Tortoise -> Turtle Shell" }
};

/* Returns 1 only when both rule creation and registration succeed. */
static int add_guaranteed_drop(
    patch_handle_t database,
    int npc_id,
    int item_id
) {
    patch_handle_t rule = PATCH_NULL;

    /*
     * ItemDropRule.Common(itemId, chanceDenominator, minimumDropped, maximumDropped)
     * chanceDenominator=1 means 100%, min=max=1 means exactly one guaranteed copy.
     */
    int chance_denominator = 1;
    int min_stack = 1;
    int max_stack = 1;

    void* common_args[4] = {
        &item_id,
        &chance_denominator,
        &min_stack,
        &max_stack
    };

    if (!patchlib_method_invoke_args(g_common, NULL, &rule, common_args) || !rule) {
        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",
            "Failed to create ItemDropRule.Common: npc=%d item=%d",
            npc_id,
            item_id
        );
        return 0;
    }

    void* register_args[2] = {
        &npc_id,
        &rule
    };

    if (!patchlib_method_invoke_args(g_register, database, NULL, register_args)) {
        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",
            "Failed to RegisterToNPC: npc=%d item=%d",
            npc_id,
            item_id
        );
        return 0;
    }

    return 1;
}

static void register_guaranteed_drops(patch_handle_t database) {
    if (g_registered) {
        return;
    }

    if (!database) {
        if (g_database_wait_logs < 3) {
            mod_logger_write(
                MOD_LOG_LEVEL_WARNING,
                "LessGrind",
                "ItemDropsDB is not ready yet; guaranteed drops will retry"
            );
            ++g_database_wait_logs;
        }
        return;
    }

    if (!g_common || !g_register) {
        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",
            "Drop registration cannot start: Common=%p RegisterToNPC=%p",
            g_common,
            g_register
        );
        return;
    }

    const size_t count = sizeof(g_drops) / sizeof(g_drops[0]);
    int newly_registered = 0;

    for (size_t i = 0; i < count; ++i) {
        unsigned int bit = 1u << i;

        if ((g_registered_mask & bit) != 0) {
            continue;
        }

        if (add_guaranteed_drop(
                database,
                g_drops[i].npc_id,
                g_drops[i].item_id
            )) {

            g_registered_mask |= bit;
            ++newly_registered;

            mod_logger_write(
                MOD_LOG_LEVEL_INFO,
                "LessGrind",
                "Guaranteed drop registered: %s",
                g_drops[i].name
            );
        }
    }

    unsigned int all_mask = (1u << count) - 1u;

    if (g_registered_mask == all_mask) {
        g_registered = 1;

        mod_logger_write(
            MOD_LOG_LEVEL_INFO,
            "LessGrind",
            "Guaranteed drops ready: %zu/%zu rules registered (mask=0x%X)",
            count,
            count,
            g_registered_mask
        );
    }
    else if (newly_registered > 0) {
        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",
            "Guaranteed drops partially registered: mask=0x%X; failed rules will retry",
            g_registered_mask
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

    patch_handle_t database = PATCH_NULL;

    if (g_item_drops_db) {
        patchlib_field_get_value(g_item_drops_db, NULL, &database);
    }

    register_guaranteed_drops(database);
}

void less_grind_drops_init(void) {
    patch_handle_t rule_type =
        patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules",
            "ItemDropRule"
        );

    patch_handle_t db_type =
        patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules",
            "ItemDropDatabase"
        );

    if (!rule_type || !db_type) {
        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",
            "Drop init failed: ItemDropRule=%p ItemDropDatabase=%p",
            rule_type,
            db_type
        );
        goto done;
    }

    g_common =
        patchlib_type_get_method_by_param_count(
            rule_type,
            "Common",
            4
        );

    g_register =
        patchlib_type_get_method_by_param_count(
            db_type,
            "RegisterToNPC",
            2
        );

    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");

    if (main_type) {
        g_item_drops_db = patchlib_type_get_field(main_type, "ItemDropsDB");

        patch_handle_t update =
            patchlib_type_get_method_by_param_count(main_type, "Update", 1);

        if (!g_common || !g_register || !g_item_drops_db || !update) {
            mod_logger_write(
                MOD_LOG_LEVEL_ERROR,
                "LessGrind",
                "Drop init missing handle: Common=%p RegisterToNPC=%p ItemDropsDB=%p Update=%p",
                g_common,
                g_register,
                g_item_drops_db,
                update
            );
        }
        else {
            g_hook = patchlib_install_prepost_hook(update, NULL, on_main_update);
        }

        if (update) {
            patchlib_free(update);
        }

        patchlib_free(main_type);
    }
    else {
        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",
            "Drop init failed: Terraria.Main not found"
        );
    }

done:
    if (rule_type) {
        patchlib_free(rule_type);
    }
    if (db_type) {
        patchlib_free(db_type);
    }

    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",
        "Drop update hook: %s (id=%d)",
        g_hook == PATCH_HOOK_INVALID_ID ? "failed" : "ready",
        (int)g_hook
    );
}

void less_grind_drops_cleanup(void) {
    if (g_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_hook);
    }

    if (g_common) {
        patchlib_free(g_common);
    }
    if (g_register) {
        patchlib_free(g_register);
    }
    if (g_item_drops_db) {
        patchlib_free(g_item_drops_db);
    }

    g_common = PATCH_NULL;
    g_register = PATCH_NULL;
    g_item_drops_db = PATCH_NULL;

    g_hook = PATCH_HOOK_INVALID_ID;
    g_registered_mask = 0;
    g_registered = 0;
    g_database_wait_logs = 0;
}
