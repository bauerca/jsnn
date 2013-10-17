#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "jsnn.h"

void jsnn_index_path(jsnn_path *path, jsnntype_t value_type, jsnn_path *array) {
    path->type = JSNN_PATH_INDEX;
    path->value_type = value_type;
    path->name = NULL;
    path->index = 0;
    path->parent = array;
}

void jsnn_attr_path(jsnn_path *path, const char *name, jsnntype_t value_type, jsnn_path *object) {
    path->type = JSNN_PATH_ATTR;
    path->value_type = value_type;
    path->name = name;
    path->index = 0;
    path->parent = object;
}


/**
 * Allocates a fresh unused token from the token pull.
 */
static jsnntok_t *jsnn_alloc_token(jsnn_parser *parser, 
		jsnntok_t *tokens, size_t num_tokens) {
	jsnntok_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
	tok->parent = -1;
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsnn_fill_token(jsnntok_t *token, jsnntype_t type, 
                            jsnnpair_t pair_type, int start, int end) {
	token->type = type;
    token->pair_type = pair_type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static jsnnerr_t jsnn_parse_primitive(jsnn_parser *parser, const char *js,
		jsnntok_t *tokens, size_t num_tokens) {
	jsnntok_t *token;
	int start;

	start = parser->pos;

	for (; js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
#ifndef JSNN_STRICT
			/* In strict mode primitive must be followed by "," or "}" or "]" */
			case ':':
#endif
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSNN_ERROR_INVAL;
		}
	}
#ifdef JSNN_STRICT
	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return JSNN_ERROR_PART;
#endif

found:
	token = jsnn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSNN_ERROR_NOMEM;
	}
	jsnn_fill_token(token, JSNN_PRIMITIVE, JSNN_VALUE, start, parser->pos);
	token->parent = parser->toksuper;
	parser->pos--;
	return JSNN_SUCCESS;
}

/**
 * Filsl next token with JSON string.
 */
static jsnnerr_t jsnn_parse_string(jsnn_parser *parser, const char *js,
		jsnntok_t *tokens, size_t num_tokens, jsnnpair_t pairtype) {
	jsnntok_t *token;

	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			token = jsnn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSNN_ERROR_NOMEM;
			}
			jsnn_fill_token(token, JSNN_STRING, pairtype, start+1, parser->pos);
			token->parent = parser->toksuper;
			return JSNN_SUCCESS;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\') {
			parser->pos++;
			switch (js[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					/* TODO */
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSNN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSNN_ERROR_PART;
}

static
const char *jsnn_type_strs[] = {
    "JSNN_PRIMITIVE",
	"JSNN_OBJECT",
	"JSNN_ARRAY",
	"JSNN_STRING"
};

static
const char *jsnn_path_type_strs[] = {
    "JSNN_PATH_INDEX",
    "JSNN_PATH_ATTR"
};

static
void print_path(jsnn_path *path) {
    jsnn_path *p = path;
    char s[256], s2[256];
    s[0] = '\0';

    for (;;) {
        if (p == NULL)
            break;
        switch (p->type) {
        case JSNN_PATH_INDEX:
            strcpy(s2, s);
            strcpy(s, "#.");
            strcat(s, s2);
            break;
        case JSNN_PATH_ATTR:
            strcpy(s2, s);
            strcpy(s, p->name);
            strcat(s, ".");
            strcat(s, s2);
            break;
        }

        p = p->parent;
    }

    printf("%s", s);
}

static
void print_tail_token(const char *js, jsnntok_t *token) {
    int width = 10;

    if (token->end > width) {
        printf("...%.*s", width, js + token->end - width);
    } else {
        printf("%.*s", token->end, js);
    }
}
    

/**
 * Check that a token matches the given path by climbing the token
 * hierarchy.
 * @param token Current value token.
 * @param path  Path to check for match.
 */
static
int jsnn_match_path(const char *js, jsnntok_t *tokens, jsnntok_t *token, jsnn_path *path) {
    jsnntok_t *tok, *ptok, *ntok;
    jsnn_path *p;
#ifdef JSNN_DEBUG
    int width;
#endif
    
    p = path;
    tok = token;

    printf("\nmatching token ("); print_tail_token(js, tok);
    printf("), path ("); print_path(p);
    printf(")...\n");

    for (;;) {
        //printf("token start: %d\n", tok->start);
        //printf("token type: %d\n", tok->type);

        /* If parent token is NULL, then the current token is the root obj.
           Success! */
        if (tok->parent == -1 && p == NULL) {
            printf("  success\n");
            break;
        }
        ptok = &tokens[tok->parent];

        printf("  %s, %s\n",
            jsnn_type_strs[tok->type],
            jsnn_type_strs[p->value_type]);
        if (tok->type != p->value_type) {
            printf("  value type mismatch\n");
            return 1;
        }

        /*
        Inspect parent token: is it an object or an array?
        If object, check the attribute name of the value.
        If array, find the index at which value exists.
        */
        switch (ptok->type) {
        case JSNN_OBJECT:
            printf("  JSNN_PATH_ATTR, %s\n",
                jsnn_path_type_strs[p->type]);
            if (p->type != JSNN_PATH_ATTR) {
                printf("  fail\n");
                return 1;
            }
            /* Get attribute name token */
            ntok = &tokens[tok - 1 - tokens];
            printf("  %.*s, %s\n",
                ntok->end - ntok->start, js + ntok->start,
                p->name);
            if (strncmp(p->name, js + ntok->start, ntok->end - ntok->start) != 0)
                return 1;
            break;
        case JSNN_ARRAY:
            printf("  JSNN_PATH_INDEX, %s\n",
                jsnn_path_type_strs[p->type]);
            //printf("  %.*s\n", tok->end - tok->start, js + tok->start);
            if (p->type != JSNN_PATH_INDEX) {
                printf("  fail\n");
                return 1;
            }
            p->index = tok - ptok - 1;
            break;
        default:
            /* We have problems */
            return JSNN_ERROR_INVAL;
        }

        p = p->parent;
        tok = ptok;
    }

    return 0;
}

/**
 * @return Returns 0 if callback was called, negative if error, or 1 if
 * no path match was found for this token.
 */
static
int jsnn_do_callback(const char *js, jsnntok_t *tokens, jsnntok_t *token,
        jsnn_path_callback *callbacks, void *data) {

    jsnn_path_callback *pc;
    int match;

    if (token->pair_type != JSNN_VALUE) {
        printf("was name\n");
        print_tail_token(js, token);
        printf("\n");
        return JSNN_ERROR_INVAL;
    }

    for (pc = callbacks; pc->path != NULL; pc++) {
        match = jsnn_match_path(js, tokens, token, pc->path);
        if (match == 0) {
            pc->callback(pc->path, js + token->start, token->end - token->start, data);
            return 0;
        } else if (match < 0) {
            printf("match problem\n");
            return match;
        }
    }

    return 1;
}

/**
 * Parse JSON string and fill tokens.
 */
jsnnerr_t jsnn_parse(jsnn_parser *parser, const char *js, jsnntok_t *tokens, 
		unsigned int num_tokens, jsnn_path_callback *callbacks, void *data) {
	jsnnerr_t r;
	int i;
	jsnntok_t *token;
    jsnnpair_t pairtype;

    //printf("json: %s\n", js);

	for (; js[parser->pos] != '\0'; parser->pos++) {
		char c;
		jsnntype_t type;

		c = js[parser->pos];
		switch (c) {
			case '{': case '[':
				token = jsnn_alloc_token(parser, tokens, num_tokens);
				if (token == NULL)
					return JSNN_ERROR_NOMEM;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
					token->parent = parser->toksuper;
				}
				token->type = (c == '{' ? JSNN_OBJECT : JSNN_ARRAY);
                token->pair_type = JSNN_VALUE;
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
                if (c == '{')
                    pairtype = JSNN_NAME;
				break;
			case '}': case ']':
				type = (c == '}' ? JSNN_OBJECT : JSNN_ARRAY);
				if (parser->toknext < 1) {
					return JSNN_ERROR_INVAL;
				}
				token = &tokens[parser->toknext - 1];
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSNN_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
                        if (callbacks != NULL) {
                            r = jsnn_do_callback(js, tokens, token, callbacks, data);
                            if (r < 0) return r;
                        }
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = &tokens[token->parent];
				}
            case ',':
                if (tokens[parser->toksuper].type == JSNN_OBJECT)
                    pairtype = JSNN_NAME;
				break;
			case '\"':
				r = jsnn_parse_string(parser, js, tokens, num_tokens, pairtype);
				if (r < 0) return r;
				if (parser->toksuper != -1)
					tokens[parser->toksuper].size++;
                if (callbacks != NULL && pairtype == JSNN_VALUE) {
                    token = &tokens[parser->toknext - 1];
                    r = jsnn_do_callback(js, tokens, token, callbacks, data);
                    if (r < 0) return r;
                }
				break;
            case ':':
                pairtype = JSNN_VALUE;
                break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
#ifdef JSNN_STRICT
			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
#else
			/* In non-strict mode every unquoted value is a primitive */
			default:
#endif
				r = jsnn_parse_primitive(parser, js, tokens, num_tokens);
				if (r < 0) return r;
				if (parser->toksuper != -1)
					tokens[parser->toksuper].size++;
				break;

#ifdef JSNN_STRICT
			/* Unexpected char in strict mode */
			default:
				return JSNN_ERROR_INVAL;
#endif

		}
	}

	for (i = parser->toknext - 1; i >= 0; i--) {
		/* Unmatched opened object or array */
		if (tokens[i].start != -1 && tokens[i].end == -1) {
			return JSNN_ERROR_PART;
		}
	}

	return JSNN_SUCCESS;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens 
 * available.
 */
void jsnn_init(jsnn_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

