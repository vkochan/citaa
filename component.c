#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "citaa.h"
#include "bsdqueue.h"

struct component_head connected_components;
struct component_head components;
CHAR seen;

char *DIR[] = {"EAST", "NORTH", "WEST", "SOUTH"};
char *COMPONENT_TYPE[] = {"UNKNOWN", "LINE", "BOX"};

void
dump_component(struct component *c)
{
	struct vertex *v;
	char *dashed = "";
	char color[256];
	char *white_text = "";
	char *shape_text = "";

	color[0] = '\0';

	if (c->dashed)	dashed = ", dashed";
	if (c->has_custom_background) {
		snprintf(color, 256, ", color %x%x%x",
				 c->custom_background.r,
				 c->custom_background.g,
				 c->custom_background.b);
	}
	if (c->white_text)	white_text = ", white text";
	switch (c->shape) {
	case SHAPE_DOCUMENT:
		shape_text = ", document";
		break;
	case SHAPE_STORAGE:
		shape_text = ", storage";
		break;
	case SHAPE_IO:
		shape_text = ", I/O box";
		break;
	}

	if (c->type == CT_BOX)
		printf("%s component, area %d%s%s%s%s\n",
			   COMPONENT_TYPE[c->type], c->area,
			   dashed, color, white_text, shape_text);
	else
		printf("%s component%s%s%s%s\n",
			   COMPONENT_TYPE[c->type],
			   dashed, color, white_text, shape_text);
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
create_component(struct component_head *storage)
{
	struct component *c = malloc(sizeof(struct component));
	if (!c) croak(1, "create_component:malloc(component)");

	c->type = CT_UNKNOWN;
	c->dashed = 0;
	c->area = 0;

	c->has_custom_background = 0;
	c->white_text = 0;
	c->shape = SHAPE_NORMAL;

	TAILQ_INIT(&c->vertices);
	TAILQ_INIT(&c->text);

	if (storage)
		TAILQ_INSERT_TAIL(storage, c, list);
	return c;
}

void
calculate_loop_area(struct component *c)
{
	int min_x = INT_MAX;
	int min_y = INT_MAX;
	int area = 0;
	struct vertex *v, *min_v, *v0, *v1;
	int dir, new_dir, i;

	min_v = NULL;
	TAILQ_FOREACH(v, &c->vertices, list) {
		if (v->x < min_x) {
			min_x = v->x;
			min_y = v->y;
			min_v = v;
		} else if (v->x == min_x && v->y < min_y) {
			min_y = v->y;
			min_v = v;
		}
	}

	/* The min_v vertex is now the topmost of the leftmost vertices.
	 * Moreover, there MUST be a way to the EAST from here. */
	v0 = min_v;
	dir = EAST;
	while (1) {
		v1 = v0->e[dir];
		area += (v0->x - v1->x) * v1->y;

		if (v1 == min_v)
			break;

		for (i = 1; i >= -1; i--) {
			new_dir = (dir + i + 4) % N_DIRECTIONS;
			if (v1->e[new_dir]) {
				dir = new_dir;
				v0 = v1;
				break;
			}
		}
	}
	c->area = area;
}

void
extract_one_loop(struct vertex *v0, int dir, struct component_head *storage)
{
	struct vertex *u, *v, *u_, *v_;
	struct component *c;
	int i, new_dir;

	printf("==LOOP\n");

	u = v0;
	c = create_component(storage);
	u_ = add_vertex_to_component(c, u->y, u->x, u->c);

	while (1) {
		v = u->e[dir];
		printf("coming from (%d,%d) to (%d,%d) due %s\n",
			   u->y, u->x, v->y, v->x, DIR[dir]);
		if (v == v0)
			v_ = find_vertex_in_component(c, v->y, v->x);
		else
			v_ = add_vertex_to_component(c, v->y, v->x, v->c);
		connect_vertices(u_, v_);
		u->e[dir] = NULL;  /* remove the edge we just followed */

		if (v == v0) break;

		u = NULL;
		for (i = 1; i >= -1; i--) {
			new_dir = (dir + i + 4) % N_DIRECTIONS;
			if (v->e[new_dir]) {
				u = v;
				u_ = v_;
				dir = new_dir;
				break;
			}
		}

		if (!u)	croakx(1, "extract_one_loop: cannot decide where to go from (%d,%d)\"%c\" -> %s",
					   v->y, v->x, v->c, DIR[dir]);
	}
	calculate_loop_area(c);
	printf("loop area = %d\n", c->area);
}

void
extract_loops(struct component *o, struct component_head *storage)
{
	struct component_head tmp;
	struct component *c, *c_tmp, *c_max;
	struct vertex *v;
	int dir;

	TAILQ_INIT(&tmp);

	TAILQ_FOREACH(v, &o->vertices, list) {
		for (dir = COMPASS_FIRST; dir <= COMPASS_LAST; dir++) {
			if (v->e[dir]) {
				extract_one_loop(v, dir, &tmp);
			}
		}
	}

	c_max = NULL;
	TAILQ_FOREACH(c, &tmp, list) {
		if (!c_max || c->area > c_max->area)
			c_max = c;
	}
	TAILQ_FOREACH_SAFE(c, &tmp, list, c_tmp) {
		TAILQ_REMOVE(&tmp, c, list);
		if (c != c_max) {
			c->type = CT_BOX;
			TAILQ_INSERT_TAIL(storage, c, list);
		}
	}
}

void
extract_one_branch(struct vertex *u, struct component_head *storage)
{
	struct vertex *v = NULL, *u_, *v_;
	struct component *c;
	int dir, branch_dir, cnt;

	c = create_component(storage);
	u_ = add_vertex_to_component(c, u->y, u->x, u->c);

	printf("==BRANCH\n");

	while (1) {
		cnt = 0;
		for (dir = COMPASS_FIRST; dir <= COMPASS_LAST; dir++)
			if (u->e[dir]) {
				cnt++;
				v = u->e[dir];
				branch_dir = dir;
			}

		if (cnt == 1) {
			v_ = add_vertex_to_component(c, v->y, v->x, v->c);

			printf("coming from (%d,%d) to (%d,%d) due %s\n",
				   u->y, u->x, v->y, v->x, DIR[branch_dir]);

			connect_vertices(u_, v_);

			u->e[branch_dir] = NULL;
			v->e[(branch_dir + 2) % N_DIRECTIONS] = NULL;

			u = v;
			u_ = v_;
			continue;
		}

		break;
	}
}

void
extract_branches(struct component *o, struct component_head *storage)
{
	struct vertex *v, *v_tmp;
	struct component_head tmp;
	struct component *c, *c_tmp;
	int dir;

	TAILQ_INIT(&tmp);

again:
	TAILQ_FOREACH_SAFE(v, &o->vertices, list, v_tmp) {
		int cnt = 0;

		for (dir = COMPASS_FIRST; dir <= COMPASS_LAST; dir++)
			if (v->e[dir])
				cnt++;

		if (cnt == 1) {
			extract_one_branch(v, &tmp);
			TAILQ_REMOVE(&o->vertices, v, list);
			goto again;
		}

		if (cnt == 0)
			TAILQ_REMOVE(&o->vertices, v, list);
	}

	TAILQ_FOREACH_SAFE(c, &tmp, list, c_tmp) {
		TAILQ_REMOVE(&tmp, c, list);
		c->type = CT_LINE;
		TAILQ_INSERT_TAIL(storage, c, list);
	}
}

static int
func_component_compare(const void *ap, const void *bp)
{
	const struct component *a = *(const struct component **)ap;
	const struct component *b = *(const struct component **)bp;
	int cp;

	cp = b->area - a->area;
	if (cp)	return cp;

	if (ap < bp)	return -1;
	if (ap > bp)	return 1;
	return 0;
}

void
sort_components(struct component_head *storage)
{
	struct component *c, **all, *c_tmp;
	int cnt = 0, i = 0;

	TAILQ_FOREACH(c, storage, list) {
		cnt++;
	}

	all = malloc(sizeof(c) * cnt);
	if (!all) croak(1, "sort_components:malloc(all)");

	TAILQ_FOREACH_SAFE(c, storage, list, c_tmp) {
		all[i++] = c;
		TAILQ_REMOVE(storage, c, list);
	}

	qsort(all, cnt, sizeof(c), func_component_compare);

	for (i = 0; i < cnt; i++)
		TAILQ_INSERT_TAIL(storage, all[i], list);
	free(all);
}

struct component_head **components_by_point;

void
build_components_by_point(struct component_head *storage, int h, int w)
{
	struct component *c;
	struct vertex *v;
	int x, y;

	components_by_point = malloc(sizeof(struct component_head *)*h);
	if (!components_by_point)	croak(1, "build_components_by_point:malloc(components_by_point)");
	for (y = 0; y < h; y++) {
		components_by_point[y] = malloc(sizeof(struct component_head)*w);
		if (!components_by_point[y])
			croak(1, "build_components_by_point:malloc(components_by_point[%d])", y);
		for (x = 0; x < w; x++)
			TAILQ_INIT(&components_by_point[y][x]);
	}

	TAILQ_FOREACH(c, storage, list) {
		TAILQ_FOREACH(v, &c->vertices, list) {
			TAILQ_INSERT_TAIL(&components_by_point[v->y][v->x], c, list_by_point);
		}
	}
}

void
determine_dashed_components(struct component_head *storage, struct image *img)
{
	struct component *c, *connected;
	struct vertex *u, *v;
	int x, y;

	TAILQ_FOREACH(c, storage, list) {
		printf("====\n");
		TAILQ_FOREACH(u, &c->vertices, list) {
			if ((v = u->e[EAST])) {
				y = u->y;
				for (x = u->x; x <= v->x; x++) {
					if (img->d[y][x] == '=') {
						c->dashed = 1;
						break;
					}
				}
			}
			if (c->dashed)
				break;
			if ((v = u->e[SOUTH])) {
				x = u->x;
				for (y = u->y; y <= v->y; y++) {
					if (img->d[y][x] == ':') {
						c->dashed = 1;
						break;
					}
				}
			}
			if (c->dashed)
				break;
		}
		if (c->dashed && c->type == CT_LINE) {
			/* Propagate dashed status for all connected branches */
			TAILQ_FOREACH(v, &c->vertices, list) {
				TAILQ_FOREACH(connected, &components_by_point[v->y][v->x], list_by_point)
					if (connected->type == CT_LINE)
						connected->dashed = 1;
			}
		}
	}
}

struct component *
find_enclosing_component(struct component_head *components, int y, int x)
{
	struct component *c;
	struct vertex *u, *v;
	int cnt;

	TAILQ_FOREACH_REVERSE(c, components, component_head, list) {
		if (c->type != CT_BOX)	continue;

		cnt = 0;

		TAILQ_FOREACH(u, &c->vertices, list) {
			if ((v = u->e[SOUTH])) {
				if (u->x == x && y >= u->y && y <= v->y) {
					/* point ON the edge, consider it to be inside */
					cnt = 1;
					break;
				}

				if (x < u->x && y >= u->y && y < v->y)
					cnt++;
			}

			if ((v = u->e[EAST])) {
				if (u->y == y && x >= u->x && x <= v->x) {
					/* point ON the edge, consider it to be inside */
					cnt = 1;
					break;
				}
			}
		}

		if (cnt % 2 == 1) {
			printf("Point %d,%d is inside the following component:\n", y, x);
			dump_component(c);
			return c;
		}
	}
	printf("Point %d,%d is outside of everything\n", y, x);
	return NULL;
}
