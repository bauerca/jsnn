#ifndef __JSNN_H_
#define __JSNN_H_

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JSNN_PRIMITIVE = 0,
	JSNN_OBJECT = 1,
	JSNN_ARRAY = 2,
	JSNN_STRING = 3
} jsnntype_t;

typedef enum {
    JSNN_NAME = 0,
    JSNN_VALUE = 1
} jsnnpair_t;

typedef enum {
    JSNN_PATH_INDEX = 0,
    JSNN_PATH_ATTR = 1
} jsnnpath_t;

typedef enum {
	/* Not enough tokens were provided */
	JSNN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSNN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSNN_ERROR_PART = -3,
	/* Everything was fine */
	JSNN_SUCCESS = 0
} jsnnerr_t;


typedef struct jsnn_path jsnn_path;

typedef int jsnn_callback(jsnn_path *path, const char *value, int len, void *data);

struct jsnn_path {
    jsnnpath_t type;
    jsnntype_t value_type;
    const char *name;
    unsigned int index;
    jsnn_path *parent;
};

typedef struct {
    jsnn_path *path;
    jsnn_callback *callback;
} jsnn_path_callback;

void jsnn_index_path(jsnn_path *path, jsnntype_t value_type, jsnn_path *array);
void jsnn_attr_path(jsnn_path *path, const char *name, jsnntype_t value_type, jsnn_path *object);

/**
 * JSON token description.
 * @param		type	    type (object, array, string etc.)
 * @param       pair_type   pair type (name or value)
 * @param		start	    start position in JSON data string
 * @param		end		    end position in JSON data string
 */
typedef struct {
	jsnntype_t type;
    jsnnpair_t pair_type;
	int start;
	int end;
	int size;
	int parent;
} jsnntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jsnn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsnn_init(jsnn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 */
jsnnerr_t jsnn_parse(jsnn_parser *parser, const char *js, 
		jsnntok_t *tokens, unsigned int num_tokens,
        jsnn_path_callback *callbacks, void *data);

#endif /* __JSNN_H_ */
