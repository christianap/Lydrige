#include "../headers/structure.h"

dval *dval_int(long integer) {
	dval *d = (dval *) malloc(sizeof(dval));
	d->type = DVAL_INT;
	d->constant = 0;
	d->integer = integer;
	return(d);
}

dval *dval_double(double doub) {
	dval *v = (dval *) malloc(sizeof(dval));
	v->type = DVAL_DOUBLE;
	v->constant = 0;
	v->doub = doub;
	return(v);
}

dval *dval_error(char *str, ...) {
	dval *d = (dval *) malloc(sizeof(dval));
	d->type = DVAL_ERROR;

	va_list va;
	va_start(va, str);
	d->str = (char *) malloc(512);
	vsnprintf(d->str, 511, str, va);
	d->str = (char *) realloc(d->str, strlen(d->str) + 1);
	va_end(va);

	return d;
}

dval *dval_func(dbuiltin func, int constant) {
	dval *d = (dval *) malloc(sizeof(dval));
	d->type = DVAL_FUNC;
	d->constant = constant;
	d->func = func;
	return(d);
}

dval *dval_list(dval *elements, unsigned int count) {
	dval *d = (dval *) malloc(sizeof(dval));
	d->type = DVAL_LIST;
	d->constant = 0;
	d->count = count;
	d->elements = elements; // NOTE: This get's calloc'd in the read_eval_expr function.
	return(d);
}

dval *dval_copy(dval *d) {
	dval *v = (dval *) malloc(sizeof(dval));
	v->type = d->type;
	v->constant = d->constant;

	switch (d->type) {
		case DVAL_INT:
			v->integer = d->integer;
			break;
		case DVAL_DOUBLE:
			v->doub = d->doub;
			break;
		case DVAL_ERROR:
			v->str = d->str;
			break;
		case DVAL_FUNC:
			v->func = d->func;
			break;
		case DVAL_ANY:
			// Give back error?
			v->type = DVAL_ERROR;
			v->str = "(Interpreter Error) Cannot copy dval of type ANY.";
			break;
		default:
			v->type = DVAL_ERROR;
			v->str = "(Interpreter Error) Cannot copy dval of Unknown type.";
	}

	return(v);
}

void dval_del(dval *d) {
	free(d); // Error Handling?
}

denv *denv_new(void) {
	denv *e = (denv*) malloc(sizeof(denv));
	e->parent = NULL;
	e->map = Hashmap_create(NULL, NULL);
	if (e->map == NULL) {
		fprintf(stderr, "Error: Failed to create hashmap for variable definitions!");
		exit(EXIT_FAILURE);
	}
	return(e);
}

void denv_del(denv *e) {
	Hashmap_destroy(e->map);
	free(e);
}

int running = true;