#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "jsnn.h"


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
const char *jsnn_pair_type_strs[] = {
    "JSNN_NAME",
    "JSNN_VALUE"
};


static
void print_token(const char *js, jsnntok_t *t) {
    printf("token:\n"
        "  size: %d\n"
        "  start: %d\n"
        "  end: %d\n"
        "  type: %s\n"
        "  pairtype: %s\n"
        "  val: %.*s\n\n",
        t->size,
        t->start,
        t->end,
        jsnn_type_strs[t->type],
        jsnn_pair_type_strs[t->pair_type],
        t->end - t->start,
        js + t->start);
}

static
int strnncmp(const char *s1, size_t n1, const char *s2, size_t n2) {
    //printf("comparing %.*s with %.*s\n", n1, s1, n2, s2);
    size_t pos;
    int diff;
    for (pos = 0;; pos++) {
        if (pos == n1 && pos < n2)
            return -1;
        else if (pos == n2 && pos < n1)
            return 1;
        else if (pos == n1 && pos == n2)
            return 0;
            
        diff = s1[pos] - s2[pos];
        if (diff)
            return diff;
    }
    return 0;
}

static
jsnntok_t *jsnn_match_index(const char *js, jsnntok_t *tokens, jsnntok_t *arr_tok, int index) {
    jsnntok_t *tok;
    int this, size, i;

    if (arr_tok->type != JSNN_ARRAY) {
        return NULL;
    }

    /* Advance through tokens til we find match */
    this = arr_tok - tokens;
    size = arr_tok->size;
    i = 0;
    for (tok = arr_tok + 1; i < size; tok++) {
        if (tok->parent == this) {
            if (i == index)
                return tok;
            i++;
        }
    }

    return NULL;
}

static
jsnntok_t *jsnn_match_attr(const char *js, jsnntok_t *tokens, jsnntok_t *obj_tok, const char *name, int len) {
    jsnntok_t *tok;
    int this, nattrs, hit;

    if (obj_tok->type != JSNN_OBJECT) {
        return NULL;
    }

    /* Advance through tokens til we find match */
    this = obj_tok - tokens;
    nattrs = obj_tok->size / 2;
    hit = 0;
    for (tok = obj_tok; hit < nattrs; tok++) {
        if (tok->pair_type == JSNN_NAME && tok->parent == this) {
            if (strnncmp(js + tok->start, tok->end - tok->start, name, len) == 0)
                return tok + 1;
            hit++;
        }
    }

    return NULL;
}

jsnntok_t *jsnn_get(jsnntok_t *root, const char *path, const char *js, jsnntok_t *tokens) {
    int pos;
    int start, len, index;
    char c, *chp;
    jsnntok_t *tok;

    if (strlen(path) == 0) return NULL;

#ifdef JSNN_DEBUG
    printf("path: %s\n", path);
#endif

    tok = root;
    for (pos = 0, start = 0;;) {
        c = path[pos];

        switch (c) {
        case '.':
#ifdef JSNN_DEBUG
            printf("  found . at %d\n", pos);
#endif
            len = pos - start;
            if ((tok = jsnn_match_attr(js, tokens, tok, path + start, len)) == NULL)
                return NULL;
            start = ++pos;
            break;
        case '[':
#ifdef JSNN_DEBUG
            printf("  found [ at %d\n", pos);
#endif
            if (start > 0) {
                /* Indicates end of attr specification */
                len = pos - start;
                if ((tok = jsnn_match_attr(js, tokens, tok, path + start, len)) == NULL)
                    return NULL;
            }

            c = path[++pos];

            if (c >= '0' && c <= '9') {
                /* Bracket is an index spec. */
                index = strtol(path + pos, &chp, 10);
                if ((tok = jsnn_match_index(js, tokens, tok, index)) == NULL)
                    return NULL;
                pos += chp - (path + pos);
            } else if (c == '\'') {
                /* Bracket is an attribute spec */
                start = ++pos;
                for (pos;; pos++) {
                    c = path[pos];
                    if (c == '\'' && path[pos - 1] != '\\') {
                        len = pos - start;
                        if ((tok = jsnn_match_attr(js, tokens, tok, path + start, len)) == NULL)
                            return NULL;
                        break;
                    }
                }
            } else {
                printf("Bad char in bracket expression.\n");
                return NULL;
            }

            if (path[pos] != ']') {
                printf("Bracket not closed properly.\n");
                return NULL;
            }
            break;
        case '\0':
            if (path[pos - 1] != ']') {
                /* Grab the last attr. */
                len = pos - start;
                if ((tok = jsnn_match_attr(js, tokens, tok, path + start, len)) == NULL)
                    return NULL;
            }
            return tok;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            printf("no spaces outside of quotes\n");
            return NULL;
        default:
            pos++;
        }
    }

    printf("The great escape!\n");
    return NULL;
}



/**
 * Parse JSON string and fill tokens.
 */
jsnnerr_t jsnn_parse(jsnn_parser *parser, const char *js, jsnntok_t *tokens, 
		unsigned int num_tokens) {
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
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = &tokens[token->parent];
				}
                break;
            case ',':
                if (tokens[parser->toksuper].type == JSNN_OBJECT)
                    pairtype = JSNN_NAME;
				break;
			case '\"':
				r = jsnn_parse_string(parser, js, tokens, num_tokens, pairtype);
				if (r < 0) return r;
				if (parser->toksuper != -1)
					tokens[parser->toksuper].size++;
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


int jsnn_cmp(jsnntok_t *token, const char *json, const char *s) {
    //printf("comparing %.*s with %.*s\n", n1, s1, n2, s2);
    size_t pos, len;
    int diff;
    char c;

    len = token->end - token->start;
    for (pos = 0;; pos++) {
        c = s[pos];
        if (pos == len) {
            if (c == '\0') return 0;
            return -1;
        } else if (c == '\0') return 1;

        diff = json[token->start] - c;
        if (diff)
            return diff;
    }
    return 0;
}
