#ifndef __JSNN_H_
#define __JSNN_H_

#ifndef JSNN_MAX_DEPTH
    #define JSNN_MAX_DEPTH 128
#endif

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
	/* Not enough tokens were provided */
	JSNN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSNN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSNN_ERROR_PART = -3,
	/* Everything was fine */
	JSNN_SUCCESS = 0
} jsnnerr_t;



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
		jsnntok_t *tokens, unsigned int num_tokens);

/**
 * Extract a value from the tokens returned by the parser based on a javascript-style
 * attribute/index access syntax.
 *
 * @param   path    Path to json node (e.g. "states[2].counties[10].name")
 */
jsnntok_t *jsnn_get(jsnntok_t *root, const char *path,
        const char *json, jsnntok_t *tokens);

/**
 * Compare a null-terminated string with the string pointed to by
 * the given token. Returns 0 if equal, <0 if token string is less
 * than s, and >0 if token string is greater than s.
 */
int jsnn_cmp(jsnntok_t *token, const char *json, const char *s);

#endif /* __JSNN_H_ */
