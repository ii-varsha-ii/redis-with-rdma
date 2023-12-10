#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include<time.h>
#include <stdlib.h>
#include <unistd.h>

void onValueChanged(const char* newValue) {
    printf("Key value changed: %s\n", newValue);
}

void pollKey(redisContext* c, const char* key) {
    redisReply* reply;
    char* previousValue = NULL;

    while (1) {
        // GET the value of the key
        reply = redisCommand(c, "GET %s", key);

        if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
            fprintf(stderr, "Error getting key value or key not found\n");
            freeReplyObject(reply);
            continue;
        }

        // Check if the value has changed
        if (previousValue == NULL || strcmp(previousValue, reply->str) != 0) {
            // Trigger the callback
            onValueChanged(reply->str);

            // Update the previous value
            free(previousValue);
            previousValue = strdup(reply->str);
        }

        freeReplyObject(reply);
    }
}

int main(void) {
    redisContext* c = redisConnect("127.0.0.1", 6379);
    if (c == NULL || c->err) {
        if (c) {
            fprintf(stderr, "Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            fprintf(stderr, "Connection error: can't allocate redis context\n");
        }
        return 1;
    }

    const char* keyToPoll = "key";
    pollKey(c, keyToPoll);

    // Note: The program will not reach this point because pollKey runs in an infinite loop.

    redisFree(c);

    return 0;
}
