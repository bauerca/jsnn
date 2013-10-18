#include "jsnn.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *json = "{"
    "\"dogs\": ["
        "{"
            "\"name\": \"spot\","
            "\"breed\": \"terrier\""
        "},"
        "{"
            "\"name\": \"gracie\","
            "\"breed\": \"golden retriever\""
        "}"
    "],"
    "\"cats\": ["
        "{"
            "\"name\": \"pickles\","
            "\"breed\": \"sphynx\""
        "}"
    "]"
"}";


void print_token(jsnntok_t *t, const char *js) {
    printf("%.*s\n", t->end - t->start, js + t->start);
}

int main(int argc, char **argv) {
    int i;
    jsnn_parser parser;
    jsnntok_t tokens[256];
    jsnntok_t *cats, *dogs, *cat, *dog, *name, *breed;
    char path[256];

    // Parse the json!
    jsnn_init(&parser);
    jsnn_parse(&parser, json, tokens, 256);

    printf("cats:\n");
    cats = jsnn_get(tokens, "cats", json, tokens);
    for (i = 0; i < cats->size; i++) {
        sprintf(path, "[%d]", i);
        cat = jsnn_get(cats, path, json, tokens);
        name = jsnn_get(cat, "name", json, tokens);
        breed = jsnn_get(cat, "breed", json, tokens);
        print_token(name, json);
        print_token(breed, json);
    }

    printf("dogs:\n");
    dogs = jsnn_get(tokens, "dogs", json, tokens);
    for (i = 0; i < dogs->size; i++) {
        sprintf(path, "[%d]", i);
        dog = jsnn_get(dogs, path, json, tokens);
        name = jsnn_get(dog, "name", json, tokens);
        breed = jsnn_get(dog, "breed", json, tokens);
        print_token(name, json);
        print_token(breed, json);
    }

}
