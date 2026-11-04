#include "falcon_wasm.h"

#include <stdlib.h>
#include <string.h>

struct subscription {
    uint64_t id;
    falcon_event_handler handler;
};

static struct subscription *subscriptions;
static uint32_t subscriptionCount;
static falcon_command_handler *commands;
static uint32_t commandCount;
static falcon_task_handler *tasks;
static uint32_t taskCount;

static bool grow(void **array, uint32_t count, size_t size) {
    void *larger = realloc(*array, (count + 1) * size);
    if (larger == NULL)
        return false;
    *array = larger;
    return true;
}

static void logAt(falcon_plugin_types_log_level_t level, const char *message) {
    falcon_plugin_string_t text;
    falcon_plugin_string_set(&text, message);
    falcon_plugin_server_log(level, &text);
}

void falcon_log(const char *message) {
    logAt(FALCON_PLUGIN_TYPES_LOG_LEVEL_INFO, message);
}

void falcon_warn(const char *message) {
    logAt(FALCON_PLUGIN_TYPES_LOG_LEVEL_WARNING, message);
}

void falcon_error(const char *message) {
    logAt(FALCON_PLUGIN_TYPES_LOG_LEVEL_ERROR, message);
}

void falcon_broadcast(const char *message) {
    falcon_plugin_string_t text;
    falcon_plugin_string_set(&text, message);
    falcon_plugin_server_broadcast_message(&text);
}

bool falcon_on(uint32_t event, falcon_event_handler handler) {
    if (!grow((void **) &subscriptions, subscriptionCount, sizeof(*subscriptions)))
        return false;

    const uint64_t id = falcon_plugin_events_subscribe(event, FALCON_PLUGIN_TYPES_PRIORITY_NORMAL, false);
    if (id == 0)
        return false;

    subscriptions[subscriptionCount].id = id;
    subscriptions[subscriptionCount].handler = handler;
    subscriptionCount++;
    return true;
}

bool falcon_command(const char *name, const char *description, falcon_command_handler handler) {
    if (!grow((void **) &commands, commandCount, sizeof(*commands)))
        return false;

    falcon_plugin_commands_command_descriptor_t descriptor;
    falcon_plugin_string_set(&descriptor.name, name);
    falcon_plugin_string_set(&descriptor.description, description);
    falcon_plugin_string_set(&descriptor.usage, "");
    descriptor.permission = FALCON_PLUGIN_TYPES_COMMAND_PERMISSION_ANY;
    descriptor.handler = commandCount;
    if (!falcon_plugin_commands_register_command(&descriptor))
        return false;

    commands[commandCount++] = handler;
    return true;
}

void falcon_reply(const char *message) {
    falcon_plugin_string_t text;
    falcon_plugin_string_set(&text, message);
    falcon_plugin_commands_sender_send_message(&text);
}

bool falcon_schedule(uint64_t delay, uint64_t period, falcon_task_handler handler) {
    if (!grow((void **) &tasks, taskCount, sizeof(*tasks)))
        return false;

    if (falcon_plugin_scheduler_schedule_task(taskCount, delay, period) == 0)
        return false;

    tasks[taskCount++] = handler;
    return true;
}

bool falcon_event_player(falcon_player *player) {
    return falcon_plugin_event_event_player(player);
}

bool falcon_sender_player(falcon_player *player) {
    return falcon_plugin_commands_sender_player(player);
}

void falcon_send(falcon_player player, const char *message) {
    falcon_plugin_string_t text;
    falcon_plugin_string_set(&text, message);
    falcon_plugin_players_player_send_message(player, &text);
}

const char *falcon_player_name(falcon_player player, char *buffer, uint32_t size) {
    falcon_plugin_string_t name;
    falcon_plugin_players_player_name(player, &name);
    const size_t length = size == 0 ? 0 : (name.len < size - 1 ? name.len : size - 1);
    if (size > 0) {
        memcpy(buffer, name.ptr, length);
        buffer[length] = '\0';
    }
    falcon_plugin_string_free(&name);
    return buffer;
}

__attribute__((weak)) void falcon_disable(void) {
}

void exports_falcon_plugin_plugin_on_load(void) {
}

bool exports_falcon_plugin_plugin_on_enable(void) {
    return falcon_enable();
}

void exports_falcon_plugin_plugin_on_disable(void) {
    falcon_disable();
}

void exports_falcon_plugin_plugin_on_event(uint64_t subscription) {
    for (uint32_t index = 0; index < subscriptionCount; index++) {
        if (subscriptions[index].id == subscription) {
            subscriptions[index].handler();
            return;
        }
    }
}

bool exports_falcon_plugin_plugin_on_command(uint32_t handler, falcon_plugin_list_string_t *arguments) {
    // binding strings are not nul terminated: copied into c strings for argv
    const size_t count = arguments->len;
    const char **argv = calloc(count + 1, sizeof(*argv));
    bool handled = false;
    bool copied = argv != NULL;
    for (size_t index = 0; copied && index < count; index++) {
        char *argument = malloc(arguments->ptr[index].len + 1);
        copied = argument != NULL;
        if (copied) {
            memcpy(argument, arguments->ptr[index].ptr, arguments->ptr[index].len);
            argument[arguments->ptr[index].len] = '\0';
            argv[index] = argument;
        }
    }

    if (copied && handler < commandCount)
        handled = commands[handler]((int) count, argv);

    for (size_t index = 0; argv != NULL && index < count; index++)
        free((char *) argv[index]);
    free(argv);
    falcon_plugin_list_string_free(arguments);
    return handled;
}

void exports_falcon_plugin_plugin_on_task(uint64_t token) {
    if (token < taskCount)
        tasks[token]();
}

void exports_falcon_plugin_plugin_on_service(falcon_plugin_string_t *name, falcon_plugin_string_t *request,
                                             falcon_plugin_string_t *ret) {
    falcon_plugin_string_free(name);
    falcon_plugin_string_free(request);
    falcon_plugin_string_dup(ret, "");
}

bool exports_falcon_plugin_plugin_on_item_use(uint32_t handler, exports_falcon_plugin_plugin_player_t player,
                                              exports_falcon_plugin_plugin_borrow_item_t item) {
    (void) handler;
    (void) player;
    (void) item;
    return false;
}

bool exports_falcon_plugin_plugin_on_item_use_on_block(uint32_t handler, exports_falcon_plugin_plugin_player_t player,
                                                       exports_falcon_plugin_plugin_borrow_item_t item,
                                                       exports_falcon_plugin_plugin_block_pos_t *position,
                                                       uint32_t face) {
    (void) handler;
    (void) player;
    (void) item;
    (void) position;
    (void) face;
    return false;
}

bool exports_falcon_plugin_plugin_on_block_interact(uint32_t handler, exports_falcon_plugin_plugin_player_t player,
                                                    exports_falcon_plugin_plugin_block_pos_t *position, uint32_t face) {
    (void) handler;
    (void) player;
    (void) position;
    (void) face;
    return false;
}

void exports_falcon_plugin_plugin_on_block_break(uint32_t handler, exports_falcon_plugin_plugin_player_t player,
                                                 exports_falcon_plugin_plugin_block_pos_t *position) {
    (void) handler;
    (void) player;
    (void) position;
}

void exports_falcon_plugin_plugin_on_entity_tick(uint32_t handler, exports_falcon_plugin_plugin_entity_t entity) {
    (void) handler;
    (void) entity;
}

bool exports_falcon_plugin_plugin_on_entity_interact(uint32_t handler, exports_falcon_plugin_plugin_entity_t entity,
                                                     exports_falcon_plugin_plugin_player_t player) {
    (void) handler;
    (void) entity;
    (void) player;
    return false;
}
