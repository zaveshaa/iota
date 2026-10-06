#ifndef JSON_H
#define JSON_H

typedef enum { JNULL, JBOOL, JNUM, JSTR, JARR, JOBJ } JKind;

typedef struct JVal JVal;
struct JVal {
    JKind kind;
    double num;
    const char *str;
    const char *key;
    JVal *child;
    JVal *next;
    int count;
};

// strings point into src and are cut at the closing quote, so src must be writable
JVal *json_parse(const char *src);
JVal *json_get(const JVal *obj, const char *key);
JVal *json_at(const JVal *arr, int i);
int json_floats(const JVal *arr, float *out, int n);
void json_free(void);

#endif
