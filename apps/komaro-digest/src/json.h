/* Minimal JSON parser: enough for InfluxDB's /query responses. */
#ifndef JSON_H
#define JSON_H

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } jtype_t;

typedef struct jnode {
    jtype_t type;
    double num;           /* J_NUM, J_BOOL (0 or 1) */
    char *str;            /* J_STR */
    char *key;            /* set when the node is a member of an object */
    struct jnode *child;  /* first element or member */
    struct jnode *next;   /* next sibling */
    int count;            /* number of elements or members */
} jnode_t;

/* Parse a complete JSON document. Returns NULL on a syntax error. */
jnode_t *json_parse(const char *text);
void json_free(jnode_t *n);

/* Object member by key; NULL if absent or n is not an object. */
const jnode_t *json_get(const jnode_t *obj, const char *key);
/* Array element by index; NULL if out of range or n is not an array. */
const jnode_t *json_at(const jnode_t *arr, int idx);

#endif
