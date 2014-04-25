#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "citaa.h"
#include "bsdqueue.h"

struct component_head connected_components;
struct component_head components;
CHAR seen;

void
dump_component(struct component *c)
{
	struct vertex *v;

	printf("components type %d, dashed %d\n", c->type, c->dashed);
	TAILQ_FOREACH(v, &c->vertices, list) {
		dump_vertex(v);
	}
}

void
compactify_component(struct component *c)
{
	struct vertex *v, *v_tmp;

	TAILQ_FOREACH_SAFE(v, &c->vertices, list, v_tmp) {
		switch (v->c) {
		case '-':
		case '=':
			// broken_if(v->e[NORTH], "%c(%d,%d) vertex has a northern edge", v->c, v->y, v->x);
			// broken_if(v->e[SOUTH], "%c(%d,%d) vertex has a southern edge", v->c, v->y, v->x);

			if (v->e[WEST] && v->e[EAST]) {
				v->e[WEST]->e[EAST] = v->e[EAST];
				v->e[EAST]->e[WEST] = v->e[WEST];
				TAILQ_REMOVE(&c->vertices, v, list);
				free(v);
			}
			break;
		case '|':
		case ':':
			// broken_if(v->e[WEST], "%c(%d,%d) vertex has a western edge", v->c, v->y, v->x);
			// broken_if(v->e[EAST], "%c(%d,%d) vertex has an eastern edge", v->c, v->y, v->x);

			if (v->e[NORTH] && v->e[SOUTH]) {
				v->e[NORTH]->e[SOUTH] = v->e[SOUTH];
				v->e[SOUTH]->e[NORTH] = v->e[NORTH];
				TAILQ_REMOVE(&c->vertices, v, list);
				free(v);
			}
			break;
		}
	}
}

struct component *
maybe_create_component(struct component *c)
{
	if (c) return c;

	c = malloc(sizeof(struct component));
	if (!c) croak(1, "maybe_create_component:malloc(component)");

	c->type = CT_UNKNOWN;
	c->dashed = 0;
	TAILQ_INIT(&c->vertices);

	TAILQ_INSERT_TAIL(&connected_components, c, list);
	return c;
}

void
extract_loops(struct component *o, struct component_head *storage)
{
	struct component_head tmp;

	TAILQ_INIT(&tmp);
}

int
main(int argc, char **argv)
{
	struct image *orig, *blob, *no_holes, *eroded, *dilated, *status;
	struct component *c;
	int x, y, i;

	orig = read_image(stdin);

	dump_image("original", orig);

	status = create_image(orig->h, orig->w, ST_EMPTY);
	TAILQ_INIT(&connected_components);
	seen = ST_SEEN;

	for (y = 0; y < orig->h; y++) {
		for (x = 0; x < orig->w; x++) {
			trace_component(orig, status, NULL, y, x);
		}
	}
	dump_image("status", status);

	TAILQ_INIT(&components);
	TAILQ_FOREACH(c, &connected_components, list) {
		compactify_component(c);
		// extract_branches(c, &components);
		// extract_loops(c, &components);
	}
	TAILQ_FOREACH(c, &components, list) {
		dump_component(c);
	}

	return 0;

	blob = copy_image(orig);
	for (i = 0; i < blob->h; i++) {
		CHAR *s = blob->d[i];
		while (*s) {
			if (*s == '+' ||
				*s == '-' ||
				*s == '|' ||
				*s == ':' ||
				*s == '=' ||
				*s == '*' ||
				*s == '/' ||
				*s == '\\' ||
				*s == '>' ||
				*s == '<' ||
				*s == '^' ||
				*s == 'V')
			{
				*s = '+';
			} else {
				*s = ' ';
			}
			s++;
		}
	}

	dump_image("blob", blob);

	// lag = build_lag(blob, '+');
	// find_lag_components(lag, 1, 1);
	// dump_lag(lag);

	no_holes = image_fill_holes(blob, 4);
	dump_image("no holes 4", no_holes);

	eroded = image_erode(no_holes, 8);
	dump_image("eroded 8", eroded);

	dilated = image_dilate(eroded, 8);
	dump_image("dilated 8", dilated);

	return 0;
}

void
dump_image(const char *title, struct image *img)
{
	int i;
	printf("Image dump of \"%s\", size %dx%d\n", title, img->w, img->h);
	for (i = 0; i < img->h; i++) {
		printf("%03d: |%s|\n", i, img->d[i]);
	}
}
