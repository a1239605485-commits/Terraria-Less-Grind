#include "mod_logger.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"

static patch_handle_t g_common = PATCH_NULL;
static patch_handle_t g_register = PATCH_NULL;
static patch_handle_t g_item_drops_db = PATCH_NULL;
static patch_hook_id_t g_hook = PATCH_HOOK_INVALID_ID;
static int g_registered = 0;

static void add_guaranteed_drop(patch_handle_t database, int npc_id, int item_id) {
    patch_handle_t rule = PATCH_NULL;
    int min = 1, max = 1, chance = 1;
    void* common_args[4] = { &item_id, &min, &max, &chance };
    if (patchlib_method_invoke_args(g_common, NULL, &rule, common_args) != 0 || !rule) return;
    void* register_args[2] = { &npc_id, &rule };
    patchlib_method_invoke_args(g_register, database, NULL, register_args);
}

/* Uses ItemDropDatabase, so drops remain ordinary world drops: no inventory injection. */
static void register_guaranteed_drops(patch_handle_t database) {
    if (g_registered || !database || !g_common || !g_register) return;
    add_guaranteed_drop(database, 6,   68);   /* Eater of Souls -> Rotten Chunk */
    add_guaranteed_drop(database, 173, 1330); /* Crimera -> Vertebra */
    add_guaranteed_drop(database, 2,   33);   /* Demon Eye -> Lens */
    add_guaranteed_drop(database, 42,  209);  /* Hornet -> Stinger */
    g_registered = 1;
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind", "Guaranteed rare-material drops registered");
}

static void on_populate(patch_handle_t database, void** args, void* result,
                        const patch_method_signature_t* sig) {
    (void)args; (void)result; (void)sig;
    register_guaranteed_drops(database);
}

void less_grind_drops_init(void) {
    patch_handle_t rule_type = patchlib_type_get_type("Terraria.GameContent.ItemDropRules", "ItemDropRule");
    patch_handle_t db_type = patchlib_type_get_type("Terraria.GameContent.ItemDropRules", "ItemDropDatabase");
    if (!rule_type || !db_type) goto done;
    g_common = patchlib_type_get_method_by_param_count(rule_type, "Common", 4);
    g_register = patchlib_type_get_method_by_param_count(db_type, "RegisterToNPC", 2);
    patch_handle_t populate = patchlib_type_get_method_by_param_count(db_type, "Populate", 0);
    if (g_common && g_register && populate) g_hook = patchlib_install_prepost_hook(populate, NULL, on_populate);
    if (populate) patchlib_free(populate);

    /* Populate has normally completed before KernelLoader starts this mod.
     * Main.ItemDropsDB is the already-built vanilla database; register now. */
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (main_type) {
        g_item_drops_db = patchlib_type_get_field(main_type, "ItemDropsDB");
        patch_handle_t database = PATCH_NULL;
        if (g_item_drops_db) patchlib_field_get_value(g_item_drops_db, NULL, &database);
        if (database) register_guaranteed_drops(database);
        else mod_logger_write(MOD_LOG_LEVEL_WARNING, "LessGrind", "Main.ItemDropsDB is unavailable; waiting for Populate");
        patchlib_free(main_type);
    }
done:
    if (rule_type) patchlib_free(rule_type);
    if (db_type) patchlib_free(db_type);
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind", "Drop hook: %s",
        g_hook == PATCH_HOOK_INVALID_ID ? "failed" : "ready");
}

void less_grind_drops_cleanup(void) {
    if (g_hook != PATCH_HOOK_INVALID_ID) patchlib_uninstall_hook(g_hook);
    if (g_common) patchlib_free(g_common);
    if (g_register) patchlib_free(g_register);
    if (g_item_drops_db) patchlib_free(g_item_drops_db);
    g_common = PATCH_NULL; g_register = PATCH_NULL; g_item_drops_db = PATCH_NULL;
    g_hook = PATCH_HOOK_INVALID_ID; g_registered = 0;
}
