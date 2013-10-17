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



// Define your data structures.
struct dog {
    char name[32];
    char breed[128];
    int age;
};

struct cat {
    char name[32];
    char breed[128];
    int age;
};

struct pets {
    struct cat cats[10];
    int num_cats;
    struct dog dogs[10];
    int num_dogs;
};


/**
 * Define your callbacks that collect data from the json string.
 * These are pretty boring---just copying strings. Maybe write a macro
 * to clean this up in your code.
 */

/* Callback for an element of the dogs array. */
int add_dog(jsnn_path *path, const char *dog, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    mypets->num_dogs++;
    printf("adding dog\n");
}
int set_dog_name(jsnn_path *path, const char *name, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    strncpy(mypets->dogs[mypets->num_dogs].name, name, len);
}
int set_dog_breed(jsnn_path *path, const char *breed, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    strncpy(mypets->dogs[mypets->num_dogs].breed, breed, len);
}
int set_dog_age(jsnn_path *path, const char *age, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    mypets->dogs[mypets->num_dogs].age = atoi(age);
}

int add_cat(jsnn_path *path, const char *cat, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    mypets->num_cats++;
}
int set_cat_name(jsnn_path *path, const char *name, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    strncpy(mypets->cats[mypets->num_cats].name, name, len);
}
int set_cat_breed(jsnn_path *path, const char *breed, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    strncpy(mypets->cats[mypets->num_cats].breed, breed, len);
}
int set_cat_age(jsnn_path *path, const char *age, int len, void *data) {
    struct pets *mypets = (struct pets *)data;
    mypets->cats[mypets->num_cats].age = atoi(age);
}


int main(int argc, char **argv) {
    int i;
    struct pets mypets;
    struct cat c;
    struct dog d;
    jsnn_parser parser;
    jsnntok_t tokens[256];

    printf("%s\n", json);

    mypets.num_dogs = 0;
    mypets.num_cats = 0;

    // Declare your schema.
    jsnn_path dogs_path,
              dog_path,
              dog_name_path,
              dog_breed_path,
              dog_age_path,
    
              cats_path,
              cat_path,
              cat_name_path,
              cat_breed_path,
              cat_age_path;
    
    // Define your schema.
    jsnn_attr_path(&dogs_path, "dogs", JSNN_ARRAY, NULL);
    jsnn_index_path(&dog_path, JSNN_OBJECT, &dogs_path);
    jsnn_attr_path(&dog_name_path, "name", JSNN_STRING, &dog_path);
    jsnn_attr_path(&dog_breed_path, "breed", JSNN_STRING, &dog_path);
    jsnn_attr_path(&dog_age_path, "age", JSNN_PRIMITIVE, &dog_path);
    
    jsnn_attr_path(&cats_path, "cats", JSNN_ARRAY, NULL);
    jsnn_index_path(&cat_path, JSNN_OBJECT, &cats_path);
    jsnn_attr_path(&cat_name_path, "name", JSNN_STRING, &cat_path);
    jsnn_attr_path(&cat_breed_path, "breed", JSNN_STRING, &cat_path);
    jsnn_attr_path(&cat_age_path, "age", JSNN_PRIMITIVE, &cat_path);

    // Link callbacks to schema for data gathering.
    jsnn_path_callback callbacks[] = {
        { &dog_path, add_dog },
        { &dog_breed_path, set_dog_breed },
        { &dog_name_path, set_dog_name },
        { &dog_age_path, set_dog_age },
        { &cat_path, add_cat },
        { &cat_breed_path, set_cat_breed },
        { &cat_name_path, set_cat_name },
        { &cat_age_path, set_cat_age },
        { NULL, NULL }
    };


    // Parse the json!
    jsnn_init(&parser);
    jsnn_parse(&parser, json, tokens, 256, callbacks, &mypets);


    printf("my (%d) cats:\n", mypets.num_cats);
    for (i = 0; i < mypets.num_cats; i++)
        printf("  '%s' (%s, %d)\n",
            mypets.cats[i].name,
            mypets.cats[i].breed,
            mypets.cats[i].age
        );
    printf("my (%d) dogs:\n", mypets.num_dogs);
    for (i = 0; i < mypets.num_dogs; i++)
        printf("  '%s' (%s, %d)\n",
            mypets.dogs[i].name,
            mypets.dogs[i].breed,
            mypets.dogs[i].age
        );
}
