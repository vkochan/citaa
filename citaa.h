#ifndef _CITAA_H
#define _CITAA_H

#include "bsdqueue.h"

#define EAST  0
#define NORTH 1
#define WEST  2
#define SOUTH 3

#define COMPASS_FIRST EAST
#define COMPASS_LAST  SOUTH

typedef char CHAR;

struct image
{
	int w, h;
	CHAR **d;
};

void chomp(CHAR *s);
void croak(int exit_code, const char *fmt, ...);
void croakx(int exit_code, const char *fmt, ...);
struct image* read_image(FILE *f);
const char *thisprogname(void);
void dump_image(const char *, struct image *);
struct image * create_image(int h, int w, CHAR init);
struct image* copy_image(struct image *);
struct image* expand_image(struct image *, int x, int y);

#define lcUnclassified  0
#define lcEdge          1
#define lcSmall         2
#define lcLarge         3
#define lcNormal        10    /* from this code all 'normal' codes begin */

/* Primitive line - building block of a LAG */

typedef struct _LAGLine
{
    int beg;
    int end;
    int code;
    int y;
    struct _LAGLine *next;    /* Next with the same code */
} LAGLine, *PLAGLine;


/*  One scan line may contain a big number of LAGLines */

/*  An example of a scan line with recognized lines indicated: */

/*     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
/*     | | |X|X|X|X| | |X|X|X|X|X|X| | |X|X| | */
/*     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
/*          ^^^^^^^     ^^^^^^^^^^^     ^^^ */
/*         line # 1       line #2     line #3 */


/* Complete LAG structure */

typedef struct _LAG
{
   int h, w;                     /* original image dimensions */
   PLAGLine *scanLines;          /* array of pointers to every scan line */
   int *lineCount;               /* number of lines (chords) in every scan line */
   int maxComponentCode;
   int codedCollectionSize;
   PLAGLine *codedLines;
   int *codedAreas;              /* Area of components is very useful */
} LAG, *PLAG;

TAILQ_HEAD(vertex_head, vertex);

struct vertex
{
	TAILQ_ENTRY(vertex) list;
	int x, y;
	CHAR c;
	struct vertex *e[4];  /* edges in four directions */
};

struct component
{
	TAILQ_ENTRY(component) list;
	int type;
	int dashed;
	struct vertex_head vertices;
};
TAILQ_HEAD(component_head, component);

void free_lag( PLAG lag);
PLAG build_lag( struct image *im, CHAR c);
void find_lag_components( PLAG lag, int edgeSize, int eightConnectivity);
void dump_lag( PLAG lag);
struct image *image_fill_holes( struct image *in, int neighborhood);
struct image *image_erode( struct image *in, int neighborhood);
struct image *image_dilate( struct image *in, int neighborhood);
struct vertex *add_vertex(struct vertex_head *vs, int y, int x, CHAR c);
struct vertex *add_vertex_to_component(struct component *c, int y, int x, CHAR ch);
void connect_vertices(struct vertex *v1, struct vertex *v2);
void dump_vertex(struct vertex *v);
struct vertex *find_vertex_in_component(struct component *c, int y, int x);

#endif