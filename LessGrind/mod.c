#include <stddef.h>
#include "mod_core.h"
#include "mod_logger.h"

void (*mod_logger_write)(mod_log_level_t level, const char* tag, const char* fmt, ...) = NULL;

void less_grind_recipes_init(void);
void less_grind_recipes_cleanup(void);
void less_grind_drops_init(void);
void less_grind_drops_cleanup(void);

static void init_mod(kernel_mod_handle_t* handle) {
    (void)handle;
    less_grind_recipes_init();
    less_grind_drops_init();
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind", "v1.0.0 loaded");
}

static void cleanup_mod(kernel_mod_handle_t* handle) {
    (void)handle;
    less_grind_drops_cleanup();
    less_grind_recipes_cleanup();
    mod_logger_write(MOD_LOG_LEVEL_INFO, "LessGrind", "unloaded");
}

static kernel_mod_info_t g_info = {
    .pkg_id = "celso.lessgrind", .version_code = 202608271,
    .api_version = 1, .version = "1.0.1"
};
static kernel_mod_info_t* get_info(void) { return &g_info; }
static kernel_mod_ops_t g_ops = { init_mod, cleanup_mod, get_info };
kernel_mod_ops_t* create_kernel_mod(void) { return &g_ops; }
