// wellness.c
#include "db.h"
#include "mongoose.h"  // Install via: https://github.com/cesanta/mongoose

static const char *s_http_port = "8000";

// JSON response helper
static void send_json(struct mg_connection *c, const char *json) {
    mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", json);
}

// GET /api/services
static void handle_services(struct mg_connection *c) {
    const char *sql = "SELECT * FROM services;";
    cJSON *json_array = cJSON_CreateArray();
    
    execute_sql(sql, [](void *data, int argc, char **argv, char **col_names) {
        cJSON *json_array = (cJSON *)data;
        cJSON *item = cJSON_CreateObject();
        for (int i = 0; i < argc; i++) {
            cJSON_AddStringToObject(item, col_names[i], argv[i] ? argv[i] : "NULL");
        }
        cJSON_AddItemToArray(json_array, item);
        return 0;
    }, json_array);

    char *json_str = cJSON_Print(json_array);
    send_json(c, json_str);
    free(json_str);
    cJSON_Delete(json_array);
}

// POST /api/book
static void handle_booking(struct mg_connection *c, struct mg_http_message *hm) {
    char user_id[10], service_id[10], date[20];
    mg_http_get_var(&hm->body, "user_id", user_id, sizeof(user_id));
    mg_http_get_var(&hm->body, "service_id", service_id, sizeof(service_id));
    mg_http_get_var(&hm->body, "date", date, sizeof(date));

    char sql[256];
    snprintf(sql, sizeof(sql), 
             "INSERT INTO bookings VALUES (%s, %s, '%s');", user_id, service_id, date);
    execute_sql(sql, NULL);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "booked");
    send_json(c, cJSON_Print(resp));
    cJSON_Delete(resp);
}

// Event handler for HTTP requests
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_http_match_uri(hm, "/api/services")) {
            handle_services(c);
        } else if (mg_http_match_uri(hm, "/api/book")) {
            handle_booking(c, hm);
        } else {
            mg_http_reply(c, 404, NULL, "Not Found");
        }
    }
}

int main() {
    init_db();  // Initialize SQLite

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, s_http_port, fn, NULL);

    printf("Server running on http://localhost:%s\n", s_http_port);
    for (;;) mg_mgr_poll(&mgr, 1000);  // Event loop

    mg_mgr_free(&mgr);
    sqlite3_close(db);
    return 0;
}