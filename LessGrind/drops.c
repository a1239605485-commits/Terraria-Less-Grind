#include <stddef.h>

#include "mod_logger.h"

#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"


/*
 * Terraria Less Grind
 *
 * 稀有材料保底掉落。
 *
 * v1.0.3 的关键错误：
 *
 * patchlib_method_invoke_args()
 *
 * 返回 true 时代表调用成功。
 *
 * 旧版却把 true 当成错误，
 * 导致成功创建出的掉落规则直接 return。
 */


static patch_handle_t g_common = PATCH_NULL;

static patch_handle_t g_register = PATCH_NULL;

static patch_handle_t g_item_drops_db = PATCH_NULL;


static patch_hook_id_t g_hook =
    PATCH_HOOK_INVALID_ID;


/*
 * 每一个 bit 表示一个掉落规则是否已经成功注册。
 *
 * 防止某个规则失败以后重新尝试时，
 * 把之前已经成功的规则重复注册。
 */
static unsigned int g_registered_mask = 0;


static int g_registered = 0;

static int g_database_wait_logs = 0;


/*
 * 一个保底掉落规则。
 */
typedef struct guaranteed_drop_t {

    int npc_id;

    int item_id;

    const char* name;

} guaranteed_drop_t;


/*
 * 当前四个保底材料。
 */
static const guaranteed_drop_t g_drops[] = {

    {
        6,
        68,
        "Eater of Souls -> Rotten Chunk"
    },

    {
        173,
        1330,
        "Crimera -> Vertebra"
    },

    {
        2,
        33,
        "Demon Eye -> Lens"
    },

    {
        42,
        209,
        "Hornet -> Stinger"
    }
};


/*
 * 添加一个保底掉落。
 *
 * 返回：
 *
 * 1 = 成功
 * 0 = 失败
 */
static int add_guaranteed_drop(
    patch_handle_t database,
    int npc_id,
    int item_id
) {

    patch_handle_t rule = PATCH_NULL;


    /*
     * ItemDropRule.Common
     *
     * 参数：
     *
     * itemId
     * chanceDenominator
     * minimumDropped
     * maximumDropped
     *
     * 这里全部设置为1：
     *
     * 1/1 = 100%
     * 最少1
     * 最多1
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


    /*
     * ===============================
     * v1.0.3 的核心错误
     * ===============================
     *
     * 原来：
     *
     * if (
     *     patchlib_method_invoke_args(...) != 0
     *     || !rule
     * )
     *
     * true 成功，
     * 却被当成失败。
     *
     * 现在：
     *
     * !patchlib_method_invoke_args(...)
     *
     * false 才失败。
     */
    if (
        !patchlib_method_invoke_args(
            g_common,
            NULL,
            &rule,
            common_args
        )
        ||
        !rule
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",

            "Failed to create "
            "ItemDropRule.Common: "
            "npc=%d item=%d",

            npc_id,
            item_id
        );


        return 0;
    }


    /*
     * RegisterToNPC(
     *     npcId,
     *     rule
     * )
     */
    void* register_args[2] = {

        &npc_id,

        &rule
    };


    if (
        !patchlib_method_invoke_args(
            g_register,
            database,
            NULL,
            register_args
        )
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",

            "Failed to RegisterToNPC: "
            "npc=%d item=%d",

            npc_id,
            item_id
        );


        return 0;
    }


    return 1;
}


/*
 * 注册四个保底规则。
 */
static void register_guaranteed_drops(
    patch_handle_t database
) {

    /*
     * 全部完成以后，
     * 后续 Update 不再执行。
     */
    if (g_registered) {
        return;
    }


    /*
     * ItemDropsDB 可能游戏刚启动时还没有创建。
     *
     * 不设置 g_registered，
     * 后面继续重试。
     */
    if (!database) {

        if (g_database_wait_logs < 3) {

            mod_logger_write(
                MOD_LOG_LEVEL_WARNING,
                "LessGrind",

                "ItemDropsDB is not ready yet; "
                "guaranteed drops will retry"
            );


            ++g_database_wait_logs;
        }


        return;
    }


    /*
     * 方法句柄必须存在。
     */
    if (
        !g_common ||
        !g_register
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",

            "Drop registration cannot start: "
            "Common=%p RegisterToNPC=%p",

            g_common,
            g_register
        );


        return;
    }


    const size_t count =
        sizeof(g_drops)
        /
        sizeof(g_drops[0]);


    int newly_registered = 0;


    /*
     * 逐个注册。
     */
    for (size_t i = 0; i < count; ++i) {

        unsigned int bit =
            1u << i;


        /*
         * 已经成功注册，
         * 不重复添加。
         */
        if (
            (g_registered_mask & bit) != 0
        ) {

            continue;
        }


        if (
            add_guaranteed_drop(
                database,
                g_drops[i].npc_id,
                g_drops[i].item_id
            )
        ) {

            /*
             * 标记该条已经完成。
             */
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


    /*
     * 例如4条规则：
     *
     * 0001
     * 0010
     * 0100
     * 1000
     *
     * 全部成功：
     *
     * 1111 = 0xF
     */
    unsigned int all_mask =
        (1u << count) - 1u;


    if (
        g_registered_mask == all_mask
    ) {

        g_registered = 1;


        mod_logger_write(
            MOD_LOG_LEVEL_INFO,
            "LessGrind",

            "Guaranteed drops ready: "
            "%zu/%zu rules registered "
            "(mask=0x%X)",

            count,
            count,
            g_registered_mask
        );
    }


    /*
     * 如果只有部分成功，
     * 后续 Update 继续尝试剩下的。
     */
    else if (newly_registered > 0) {

        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",

            "Guaranteed drops partially registered: "
            "mask=0x%X; "
            "failed rules will retry",

            g_registered_mask
        );
    }
}


/*
 * Main.Update Postfix
 */
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


    patch_handle_t database =
        PATCH_NULL;


    /*
     * Main.ItemDropsDB
     */
    if (g_item_drops_db) {

        patchlib_field_get_value(
            g_item_drops_db,
            NULL,
            &database
        );
    }


    register_guaranteed_drops(
        database
    );
}


/*
 * 初始化掉落模块。
 */
void less_grind_drops_init(void) {

    /*
     * ItemDropRule
     */
    patch_handle_t rule_type =

        patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules",
            "ItemDropRule"
        );


    /*
     * ItemDropDatabase
     */
    patch_handle_t db_type =

        patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules",
            "ItemDropDatabase"
        );


    /*
     * 类型不存在。
     */
    if (
        !rule_type ||
        !db_type
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",

            "Drop init failed: "
            "ItemDropRule=%p "
            "ItemDropDatabase=%p",

            rule_type,
            db_type
        );


        goto done;
    }


    /*
     * ItemDropRule.Common
     *
     * 参数数量4。
     */
    g_common =

        patchlib_type_get_method_by_param_count(
            rule_type,
            "Common",
            4
        );


    /*
     * ItemDropDatabase.RegisterToNPC
     *
     * 参数数量2。
     */
    g_register =

        patchlib_type_get_method_by_param_count(
            db_type,
            "RegisterToNPC",
            2
        );


    /*
     * Terraria.Main
     */
    patch_handle_t main_type =

        patchlib_type_get_type(
            "Terraria",
            "Main"
        );


    if (main_type) {

        /*
         * Terraria.Main.ItemDropsDB
         */
        g_item_drops_db =

            patchlib_type_get_field(
                main_type,
                "ItemDropsDB"
            );


        /*
         * Main.Update(GameTime)
         */
        patch_handle_t update =

            patchlib_type_get_method_by_param_count(
                main_type,
                "Update",
                1
            );


        /*
         * 检查所有句柄。
         */
        if (
            !g_common ||
            !g_register ||
            !g_item_drops_db ||
            !update
        ) {

            mod_logger_write(
                MOD_LOG_LEVEL_ERROR,
                "LessGrind",

                "Drop init missing handle: "
                "Common=%p "
                "RegisterToNPC=%p "
                "ItemDropsDB=%p "
                "Update=%p",

                g_common,
                g_register,
                g_item_drops_db,
                update
            );
        }

        else {

            /*
             * 安装 Postfix。
             */
            g_hook =

                patchlib_install_prepost_hook(
                    update,
                    NULL,
                    on_main_update
                );
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

            "Drop init failed: "
            "Terraria.Main not found"
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

        g_hook == PATCH_HOOK_INVALID_ID
            ? "failed"
            : "ready",

        (int)g_hook
    );
}


/*
 * 清理。
 */
void less_grind_drops_cleanup(void) {

    if (
        g_hook != PATCH_HOOK_INVALID_ID
    ) {

        patchlib_uninstall_hook(
            g_hook
        );
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


    g_hook =
        PATCH_HOOK_INVALID_ID;


    g_registered_mask = 0;

    g_registered = 0;

    g_database_wait_logs = 0;
}
