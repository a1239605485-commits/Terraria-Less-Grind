#include <stddef.h>
#include "mod_logger.h"
#include "tefkernel/patchlib/field.h"
#include "tefkernel/patchlib/method.h"
#include "tefkernel/patchlib/struct/array.h"

/*
 * Terraria Less Grind - recipe tweaks
 *
 * Important TEFKernel API rule:
 *   patchlib_array_at(...) returns bool: true = success, false = failure.
 *
 * The old v1.0.3 code used "!= 0" as the failure condition, which skipped
 * every successfully-read array element. This version fixes that logic and
 * adds logging to tell whether Terraria's recipe table was actually changed.
 */

static patch_handle_t g_main_recipes = PATCH_NULL;
static patch_handle_t g_recipe_create = PATCH_NULL;
static patch_handle_t g_recipe_required = PATCH_NULL;
static patch_handle_t g_item_type = PATCH_NULL;
static patch_handle_t g_item_stack = PATCH_NULL;

static patch_hook_id_t g_hook = PATCH_HOOK_INVALID_ID;

static int g_applied = 0;
static int g_wait_log_count = 0;


static int contains(const int* values, size_t count, int value) {
    for (size_t i = 0; i < count; ++i) {
        if (values[i] == value) {
            return 1;
        }
    }

    return 0;
}


/*
 * Boss召唤物材料减半。
 *
 * 返回真正被修改的材料数量。
 */
static int halve_ingredients(patch_handle_t recipe) {
    patch_handle_t items = PATCH_NULL;

    patchlib_field_get_value(
        g_recipe_required,
        recipe,
        &items
    );

    if (!items) {
        return 0;
    }

    size_t length = patchlib_array_length(items);

    int changed = 0;

    for (size_t i = 0; i < length; ++i) {

        patch_handle_t ingredient = PATCH_NULL;

        /*
         * 修复：
         *
         * patchlib_array_at 返回 true 代表成功。
         *
         * v1.0.3 原代码：
         *
         * if (patchlib_array_at(...) != 0 || !ingredient)
         *
         * 会把成功读取当成失败。
         */
        if (!patchlib_array_at(
                items,
                i,
                &ingredient
            ) || !ingredient) {

            continue;
        }

        int stack = 0;

        patchlib_field_get_value(
            g_item_stack,
            ingredient,
            &stack
        );

        if (stack > 1) {

            /*
             * 向上取整。
             *
             * 例如：
             * 10 -> 5
             * 5  -> 3
             * 3  -> 2
             */
            int cheaper = (stack + 1) / 2;

            patchlib_field_set_value(
                g_item_stack,
                ingredient,
                &cheaper
            );

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

    patchlib_field_get_value(
        g_main_recipes,
        NULL,
        &recipes
    );


    /*
     * 游戏启动较早阶段 Main.recipe 可能还没准备好。
     *
     * 这里不能设置 g_applied，
     * 后面继续重试。
     */
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


    /*
     * ================================
     * 建筑材料
     * ================================
     *
     * 当前保留 v1.0.3 的物品表。
     *
     * 先验证读取配方和修改配方功能是否恢复。
     */
    static const int building_items[] = {

        1, 2, 3, 4, 5,
        6, 7, 8, 9, 10,
        11, 12, 13, 14, 15,
        16, 17, 18, 19,

        22,

        25, 26, 27, 28, 29,
        30, 31, 32,

        38, 39, 40, 41, 42,

        94, 95, 96
    };


    /*
     * ================================
     * 常用药水
     * ================================
     */
    static const int common_potions[] = {

        126,
        227,

        288, 289, 290, 291,
        292, 293, 294, 295,
        296, 297, 298, 299,
        300, 301, 302, 303,
        304, 305, 306,

        2322,
        2323
    };


    /*
     * ================================
     * Boss召唤物
     * ================================
     */
    static const int boss_summons[] = {

        43,
        267,
        544,
        556,
        557,
        560,
        1133,
        1293,
        1330
    };


    int building = 0;
    int potions = 0;
    int summons = 0;

    int summon_ingredients = 0;

    size_t read_ok = 0;
    size_t read_failed = 0;


    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",
        "Recipe scan started: total=%zu",
        total
    );


    for (size_t i = 0; i < total; ++i) {

        patch_handle_t recipe = PATCH_NULL;
        patch_handle_t result = PATCH_NULL;


        /*
         * ==========================
         * v1.0.3 最关键 BUG
         * ==========================
         *
         * 原来：
         *
         * if (patchlib_array_at(...) != 0)
         *
         * 成功反而 continue。
         *
         * 现在：
         *
         * !patchlib_array_at(...)
         *
         * 只有失败才 continue。
         */
        if (!patchlib_array_at(
                recipes,
                i,
                &recipe
            ) || !recipe) {

            ++read_failed;

            continue;
        }


        ++read_ok;


        /*
         * 获取：
         *
         * Recipe.createItem
         */
        patchlib_field_get_value(
            g_recipe_create,
            recipe,
            &result
        );


        if (!result) {
            continue;
        }


        int type = 0;
        int stack = 0;


        /*
         * 获取产物 Item.type
         */
        patchlib_field_get_value(
            g_item_type,
            result,
            &type
        );


        /*
         * 获取产物 Item.stack
         */
        patchlib_field_get_value(
            g_item_stack,
            result,
            &stack
        );


        /*
         * ====================================
         * 功能1：
         *
         * 建筑材料合成数量 ×3
         * ====================================
         */
        if (contains(
                building_items,
                sizeof(building_items)
                    / sizeof(building_items[0]),
                type
            )) {

            int amount;

            if (stack > 0) {
                amount = stack * 3;
            }
            else {
                amount = 3;
            }


            patchlib_field_set_value(
                g_item_stack,
                result,
                &amount
            );


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


        /*
         * ====================================
         * 功能2：
         *
         * 常用药水批量制作 ×5
         * ====================================
         */
        else if (contains(
                common_potions,
                sizeof(common_potions)
                    / sizeof(common_potions[0]),
                type
            )) {

            int amount;

            if (stack > 0) {
                amount = stack * 5;
            }
            else {
                amount = 5;
            }


            patchlib_field_set_value(
                g_item_stack,
                result,
                &amount
            );


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


        /*
         * ====================================
         * 功能3：
         *
         * Boss召唤物制作材料减半
         * ====================================
         */
        else if (contains(
                boss_summons,
                sizeof(boss_summons)
                    / sizeof(boss_summons[0]),
                type
            )) {

            int changed =
                halve_ingredients(recipe);


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
    }


    /*
     * 必须确认至少成功读取了一个 Recipe
     * 才认为本次扫描真正完成。
     *
     * 这样不会再出现：
     *
     * 第一次读取失败
     * ↓
     * g_applied = 1
     * ↓
     * 后面永远不尝试
     */
    if (read_ok > 0) {
        g_applied = 1;
    }


    /*
     * 最重要的总诊断信息。
     *
     * 下一次测试时重点看这条。
     */
    mod_logger_write(
        MOD_LOG_LEVEL_INFO,
        "LessGrind",

        "Recipe scan finished: "
        "total=%zu, "
        "read_ok=%zu, "
        "read_failed=%zu, "
        "building=%d, "
        "potion=%d, "
        "boss_summon=%d, "
        "summon_ingredients=%d, "
        "applied=%d",

        total,
        read_ok,
        read_failed,
        building,
        potions,
        summons,
        summon_ingredients,
        g_applied
    );


    /*
     * 如果能读取 Recipe，
     * 但是一个目标都没找到，
     * 下一步就说明不是 Hook 问题，
     * 而是 Terraria 1.4.5.6.4 的 item ID 表需要重新整理。
     */
    if (
        read_ok > 0 &&
        building == 0 &&
        potions == 0 &&
        summons == 0
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_WARNING,
            "LessGrind",

            "Recipe table was readable, "
            "but no configured item IDs matched. "
            "If gameplay is still unchanged, "
            "update the item-ID lists for this Terraria build."
        );
    }
}


/*
 * Main.Update 后置 Hook
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


    apply_recipe_changes();
}


void less_grind_recipes_init(void) {

    patch_handle_t main_type =
        patchlib_type_get_type(
            "Terraria",
            "Main"
        );


    patch_handle_t recipe_type =
        patchlib_type_get_type(
            "Terraria",
            "Recipe"
        );


    patch_handle_t item_type =
        patchlib_type_get_type(
            "Terraria",
            "Item"
        );


    /*
     * 基础类型检测
     */
    if (
        !main_type ||
        !recipe_type ||
        !item_type
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",

            "Recipe init failed: "
            "required Terraria type not found "
            "(Main=%p Recipe=%p Item=%p)",

            main_type,
            recipe_type,
            item_type
        );


        goto done;
    }


    /*
     * Terraria.Main.recipe
     */
    g_main_recipes =
        patchlib_type_get_field(
            main_type,
            "recipe"
        );


    /*
     * Terraria.Recipe.createItem
     */
    g_recipe_create =
        patchlib_type_get_field(
            recipe_type,
            "createItem"
        );


    /*
     * Terraria.Recipe.requiredItem
     */
    g_recipe_required =
        patchlib_type_get_field(
            recipe_type,
            "requiredItem"
        );


    /*
     * Terraria.Item.type
     */
    g_item_type =
        patchlib_type_get_field(
            item_type,
            "type"
        );


    /*
     * Terraria.Item.stack
     */
    g_item_stack =
        patchlib_type_get_field(
            item_type,
            "stack"
        );


    /*
     * 获取：
     *
     * Main.Update(GameTime)
     *
     * 参数数量 = 1
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
        !g_main_recipes ||
        !g_recipe_create ||
        !g_recipe_required ||
        !g_item_type ||
        !g_item_stack ||
        !update
    ) {

        mod_logger_write(
            MOD_LOG_LEVEL_ERROR,
            "LessGrind",

            "Recipe init missing handle: "
            "recipe=%p "
            "createItem=%p "
            "requiredItem=%p "
            "item.type=%p "
            "item.stack=%p "
            "Update=%p",

            g_main_recipes,
            g_recipe_create,
            g_recipe_required,
            g_item_type,
            g_item_stack,
            update
        );
    }

    else {

        /*
         * 安装 Main.Update Postfix Hook
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

        g_hook == PATCH_HOOK_INVALID_ID
            ? "failed"
            : "ready",

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


    g_main_recipes = PATCH_NULL;
    g_recipe_create = PATCH_NULL;
    g_recipe_required = PATCH_NULL;

    g_item_type = PATCH_NULL;
    g_item_stack = PATCH_NULL;

    g_hook = PATCH_HOOK_INVALID_ID;

    g_applied = 0;
    g_wait_log_count = 0;
}
