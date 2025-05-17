#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <json-c/json.h>

void send_json(int sock, struct json_object *jobj);
struct json_object *receive_json(int sock);

#endif 