#include <stdlib.h>

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
#ifdef JSNN_PARENT_LINKS
	tok->parent = -1;
#endif
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
#ifdef JSNN_PARENT_LINKS
	token->parent = parser->toksuper;
#endif
	parser->pos--;
	return JSNN_SUCCESS;
}

/**
 * Filsl next token with JSON string.
 */
static jsnnerr_t jsnn_parse_string(jsnn_parser *parser, const char *js,
		jsnntok_t *tokens, size_t num_tokens) {
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
			jsnn_fill_token(token, JSNN_STRING, JSNN_VALUE, start+1, parser->pos);
#ifdef JSNN_PARENT_LINKS
			token->parent = parser->toksuper;
#endif
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

/**
 * Parse JSON string and fill tokens.
 */
jsnnerr_t jsnn_parse(jsnn_parser *parser, const char *js, jsnntok_t *tokens, 
		unsigned int num_tokens) {
	jsnnerr_t r;
	int i;
	jsnntok_t *token;

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
#ifdef JSNN_PARENT_LINKS
					token->parent = parser->toksuper;
#endif
				}
				token->type = (c == '{' ? JSNN_OBJECT : JSNN_ARRAY);
                token->pair_type = JSNN_VALUE;
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
				break;
			case '}': case ']':
				type = (c == '}' ? JSNN_OBJECT : JSNN_ARRAY);
#ifdef JSNN_PARENT_LINKS
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
#else
				for (i = parser->toknext - 1; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSNN_ERROR_INVAL;
						}
						parser->toksuper = -1;
						token->end = parser->pos + 1;
						break;
					}
				}
				/* Error if unmatched closing bracket */
				if (i == -1) return JSNN_ERROR_INVAL;
				for (; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						parser->toksuper = i;
						break;
					}
				}
#endif
				break;
			case '\"':
				r = jsnn_parse_string(parser, js, tokens, num_tokens);
				if (r < 0) return r;
				if (parser->toksuper != -1)
					tokens[parser->toksuper].size++;
				break;
            case ':':
                /* Previous token should be a string. Set it as the name
                   of a pair. */
                if (parser->toksuper != -1) {
                    if (tokens[parser->toksuper].type == JSNN_OBJECT) {
                        token = &tokens[parser->toknext - 1];
                        if (token->type != JSNN_STRING) {
                            return JSNN_ERROR_INVAL;
                        }
                        token->pair_type = JSNN_NAME;
                    }
                }
                break;
			case '\t' : case '\r' : case '\n' : case ',': case ' ':
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

