/* Copyright 2008 Felix S Klock II              -*- indent-tabs-mode: nil -*-
 * 
 * $Id: $
 *
 * Summarization set matrix implementation.
 * 
 * The regions are partitioned into three parts: 
 * { uninteresting, summarized, summarizing },
 * aka "UNSZ", "SZED", and "SZING".
 * 
 * The matrix maps (SZED u SZING)  -> Col, 
 * where a Col (column) is a list of Cells and a Hashset.
 * 
 * interp: the union of a Col's Cells and its Hashset are the region's
 * points-into summary.
 * 
 * For any Col, a word may occur at most once in its Cells.  There
 * *can* be overlap between a Col's Cells and its Hashset.  If one
 * must ensure that an iteration over a Col does not repeat words, one
 * can filter the Cells iteration by the contents of the Hashset (at
 * the expense of paying for hash-lookups during the cell iteration).
 * 
 */ 

#define GC_INTERNAL

#define dbmsg( format, args... ) if (0) consolemsg( format, ## args )
#define db_printgset( prefix, arg ) if (0) console_printgset( prefix, arg )

#define assertmsg( format, args... ) if (1) consolemsg( format, ## args )
#define assert_printgset( prefix, arg ) if (1) console_printgset( prefix, arg )
#define verifymsg( format, args... ) if (0) consolemsg( format, ## args )

#include <stdio.h>
#include <math.h>

#include "larceny.h"
#include "gclib.h"
#include "summ_matrix_t.h"
#include "remset_t.h"
#include "msgc-core.h"
#include "gc_t.h"
#include "gset_t.h"
#include "locset_t.h"
#include "old_heap_t.h"
#include "region_group_t.h"
#include "seqbuf_t.h"
#include "semispace_t.h"
#include "smircy.h"
#include "uremset_t.h"

#define DEFAULT_OBJS_POOL_SIZE 2048 /* 2K elements = 8KB */
#define DEFAULT_LOCS_POOL_SIZE 1024 /* 1K elements = 8KB */

#define EXPORT

/* As this gets larger, more check_rep calls are included.
 * Rough intution is that degree 1 maintains linear growth,
 * degree 2 introduces quadratic, etc, though this is not necessarily
 * adhered to. */
#define CHECK_REP_DEGREE 0

#define FREE_CELL_ASSERTS_UNREACHABLE 0

#define ADD_TO_SUMAR_ASSERTS_UNIQ_ENQ 0
#define POOL_ENQUEUE_ASSERTS_UNIQ_ENQ 0

#define SUMMARIZE_KILLS_RS_ENTRIES 0

#define CONSERVATIVE_REGION_COUNT 0

/* cyclic FSM: sched -> doing -> finis -> sched */
/* synchronized with smircy: build -> refine -> done -> build */
typedef enum { 
  sched_refn, doing_refn, finis_refn 
} sumz_refn_t;

typedef struct locs_pool locs_pool_t;
typedef struct objs_pool objs_pool_t;
typedef struct summ_cell summ_cell_t;
typedef struct summ_row summ_row_t;
typedef struct summ_col summ_col_t;
typedef struct summ_matrix_data summ_matrix_data_t;

struct objs_pool {
  word *bot;
  word *top;
  word *lim;
  objs_pool_t *next;
};
struct locs_pool {
  loc_t *bot;
  loc_t *top;
  loc_t *lim;
  locs_pool_t *next;
};
/* REP INV: bot < lim && bot <= top <= lim.
 * interpretation: each objs_pool is filled with objects from
 * [bot,top) if next is non-null, it holds prior insertions.
 * ABS INV: all inserted objects are distinct. */

struct summ_cell {
  objs_pool_t *objects;
  locs_pool_t *locations;
  int source_gno;
  int target_gno;
  summ_cell_t *prev_row; /* left elem; sentinel cell at end */
  summ_cell_t *next_row; /* right elem; sentinel cell at end */
  summ_cell_t *prev_col; /* up   elem; sentinel cell at end */
  summ_cell_t *next_col; /* down elem; sentinel cell at end */
  int dbg_id;             /* uid (for debug output) */
};
static int summ_cell_next_dbg_id = 101;
static int next_dbg_id(void) {
  int ret = summ_cell_next_dbg_id;
  summ_cell_next_dbg_id += 1;
  return ret;
}
struct summ_row {
  summ_cell_t *cell_lft; /* sentinel node */
  summ_cell_t *cell_rgt; /* end of linked-list (sentinel if row empty) */
  int source_gno;
};

struct summ_col {
  summ_cell_t *cell_top; /* sentinel node */
  summ_cell_t *cell_bot; /* end of linked-list (sentinel if col empty) */
  int target_gno;
  int summarize_word_count; /* contribution from summarization */
  int writebarr_word_count; /* contribution from mutator (via write-barrier) */
  int collector_word_count; /* contribution from cheney (obj forwarding) */
  int summacopy_word_count; /* contribution from gc    (summary copying) */
  bool overly_popular;
  bool construction_inprogress;
  bool construction_complete;
  bool construction_predates_snapshot;

  locset_t *sum_mutator; /* objects inserted via write-barrier */
  /* XXX [pnkfelix] Tue Jan 6 16:20:31 EST 2009
   * Why is sum_mutator maintained per-column rather than globally? 
   * 
   * Because:
   *  1. I don't want to spend time during a gc scanning irrelevant
   *     objects, and
   *  2. because I would not know when it was safe to remove the elements 
   *     that point into region R after collection of R based solely
   *     a single sum_mutator shared between all the remsets
   * 
   * XXX okay, well, how can I avoid spending O(#cols) time looking at
   * every sum_mutator during sm_clear_contribution_to_summaries ?
   * I'm punting on this question in the short term.
   * 
   * Note that such a scan is either: 
   * - looking at a dense array of mostly small sum_mutators, or
   * - looking at a spare array of dense sum_mutators
   * (we can't encounter a dense array of dense sum_mutators, because
   * we'll force gc's before that many mutations occur).
   * In the former case, a scan is unavoidable, but also acceptable (it seems to me).
   * Therefore the scan in the latter case is also acceptable.
   * 
   * If #cols can grow to the point that the scan is unacceptable,
   * that would mean that we can never have a dense array of non-empty
   * sum_mutators (otherwise by definition the scan must be
   * acceptable) -- in that situation, can work around problem by
   * cons'ing up a linked list of all non-empty sum_mutators.  (I may
   * consider putting that in anyway...)
   */
};

struct summ_matrix_data {
  double coverage;
  double goal;
  double p;
  int entries_per_objs_pool_segment;
  int entries_per_locs_pool_segment;

  summ_row_t **rows;
  summ_col_t **cols;
  int num_cols;
  int num_rows;

  int col_complete_first;   /* completed summmary, or -1 if none */
  int col_complete_lim;
  int col_inprogress_first; /* under-construction, or -1 if none */
  int col_inprogress_lim;

  /* TODO: use something like the below with concert with summ_cell to
   * cache summ_cell_t lookups. */
  struct {
    int cached_gno;
    summ_cell_t **table; /* maps src_gno -> cell or NULL */
    int table_len; /* may be redundant, given gensets below... */

    int last_src_gno;
    int last_tgt_gno;
    summ_cell_t* last_cell;
    bool last_cell_valid;
  } row_cache;

  /* refactoring */
  int       summaries_count;
  int       popularity_limit;   /* Maximum summary size allowed (in words) */

  struct {

    /* TODO: factor this out.  Felix now thinks goal involves the currently
     * built summaries and the ones under construction, which means that this
     * state does not really belong here... */
    int    goal; /* number of usable summaries needed before we say "done" */

    bool   waiting;   /* TRUE only if currently delaying next round of summz */
    bool   complete;  /* TRUE only if done with current round of sumz. */
    int    cursor; /* start remset for next sm_build_summaries_partial() */
    int    num; /* #remsets to scan in each sm_build_summaries_partial() */

  } summarizing;

  locset_t *nursery_locset;     /* Points-into remset for the nursery. */

  summary_t nursery_summary;
  summary_t mutrems_summary;
  summary_t summcol_summary_locs;
  summary_t summcol_summary_objs;

  int cycle_count; /* number of cycles over program run */
  int pass_count; /* number of scans over whole heap */
  int curr_pass_units_count; /* num rgns via current pass (ie partial scan). */

  struct {
    bool      active;
    int       summaries_left_to_process;
    bool      finished_remset_pass_since_start;

    /* Idea: can discard smircy snapshot after we have both:
     *   - finished one pass over the rememmbered set since the 
     *     snapshot was completed, and
     *   - finished with all summaries present or in-progress at time
     *     snapshot was completed
     * 
     * The first condition is flagged via
     * finished_remset_pass_since_start; the second condition is
     * flagged with summaries_left_to_process reaches zero.
     */
  } refining;
};

#define DATA(sm)                ((summ_matrix_data_t*)(sm->data))

static int calc_goal( summ_matrix_t *summ, int region_count ) 
{ 
  return max(1,(int)floor(((double)region_count) * DATA(summ)->goal));
}

static objs_pool_t *alloc_objpool_segment( unsigned entries_per_pool_segment )
{
  objs_pool_t *p;
  word *heapptr;
  p = (objs_pool_t*) must_malloc( sizeof(objs_pool_t) );
  while (1) {
    heapptr = gclib_alloc_rts( entries_per_pool_segment*sizeof(word), MB_SUMMARY_SETS );
    if (heapptr != 0) break;
    memfail( MF_RTS, "Can't allocate summary matrix pool.");
  }

  p->bot = p->top = heapptr;
  p->lim = heapptr + entries_per_pool_segment;
  p->next = NULL;

  return p;
}

static locs_pool_t *alloc_locpool_segment( unsigned entries_per_pool_segment )
{
  locs_pool_t *p;
  loc_t *heapptr;
  p = (locs_pool_t*) must_malloc( sizeof(locs_pool_t) );
  while (1) {
    heapptr = gclib_alloc_rts( entries_per_pool_segment*sizeof(loc_t), MB_SUMMARY_SETS );
    if (heapptr != 0) break;
    memfail( MF_RTS, "Can't allocate summary matrix pool.");
  }

  p->bot = p->top = heapptr;
  p->lim = heapptr + entries_per_pool_segment;
  p->next = NULL;

  return p;
}

static summ_cell_t* make_cell( int source_gno, int target_gno ) {
  summ_cell_t *e;
  int dbg_id;

  dbg_id = next_dbg_id();
  dbmsg("   make_cell( src=%d, tgt=%d ) => id:%d",
        source_gno, target_gno, dbg_id );

  e = (summ_cell_t*) must_malloc( sizeof(summ_cell_t) );
  e->objects = NULL;
  e->locations = NULL;
  e->source_gno = source_gno;
  e->target_gno = target_gno;
  e->dbg_id = dbg_id;
  return e;
}

static void free_cell( summ_cell_t *e, 
                       int entries_per_objs_pool_segment,
                       int entries_per_locs_pool_segment ) 
{
  dbmsg("   free_cell( id:%d src: %d tgt: %d, entries: %d %d )",
        e->dbg_id, e->source_gno, e->target_gno, 
        entries_per_objs_pool_segment, entries_per_locs_pool_segment );

  {
    objs_pool_t *p; 
    objs_pool_t *n;

    p = e->objects;
    while (p != NULL) {
      gclib_free( p->bot, entries_per_objs_pool_segment*sizeof(word) );
      n = p->next; /* save across free(p) call */
      free(p);
      p = n;
    }
  }

  {
    locs_pool_t *p; 
    locs_pool_t *n;

    p = e->locations;
    while (p != NULL) {
      gclib_free( p->bot, entries_per_locs_pool_segment*sizeof(loc_t) );
      n = p->next; /* save across free(p) call */
      free(p);
      p = n;
    }
  }
  free(e);
}

static summ_cell_t* make_sentinel_cell( int src_gno, int tgt_gno ) 
{
  summ_cell_t *e;
  assert( src_gno == -1 || tgt_gno == -1 );
  e = (summ_cell_t*) must_malloc( sizeof(summ_cell_t) );
  e->objects = NULL;
  e->locations = NULL;
  e->source_gno = src_gno;
  e->target_gno = tgt_gno;
  e->dbg_id = next_dbg_id();
  e->prev_row = e; /* complete       */
  e->next_row = e; /*  horizontally  */
  e->prev_col = e; /*   and          */
  e->next_col = e; /*    vertically! */
  return e;
}

static int pool_count_objects( objs_pool_t *objects ) 
{
  /* The pool for any one cell shouldn't get too deep. */
  if (objects == NULL) {
    return 0;
  } else {
    return (objects->top - objects->bot) + 
      pool_count_objects( objects->next );
  }
}

static int pool_count_locations( locs_pool_t *locations ) 
{
  /* The pool for any one cell shouldn't get too deep. */
  if (locations == NULL) {
    return 0;
  } else {
    return (locations->top - locations->bot) + 
      pool_count_locations( locations->next );
  }
}

static char* row_templ = "%-5s";
static char* col_templ = "%-5s";
static char* cell_templ = "%-27s";

static void print_cell( summ_cell_t *cell ) 
{
  printf("[%3d(%2d,%2d)%-4d+%-4d,l%3d,r%3d,u%3d,d%3d] ",
         cell->dbg_id, cell->source_gno, cell->target_gno, 
         pool_count_objects( cell->objects ),
         pool_count_locations( cell->locations ),
         cell->prev_row->dbg_id, 
         cell->next_row->dbg_id, 
         cell->prev_col->dbg_id, 
         cell->next_col->dbg_id);
}
static void print_skip_cell(void) 
{
  printf(cell_templ, "");
}

static void print_row_header( summ_row_t *row ) 
{
  printf(row_templ, "ROW");
}
static void print_col_header( summ_col_t *col )
{
  printf(col_templ, "COL");
}

static void print_row( summ_row_t *row ) 
{
  summ_cell_t *sent = row->cell_lft;
  summ_cell_t *cell = sent;
  print_row_header( row );
  do {
    print_cell( cell );
    cell = cell->next_row;
  } while ( cell != sent );
  printf("\n");
}

static void print_matrix_via_rows( summ_matrix_t *sm ) 
{
  int i;
  for (i = 0; i < DATA(sm)->num_rows; i++) {
    print_row( DATA(sm)->rows[i] );
  }
  printf("\n");
}

static void print_col( summ_col_t *col ) 
{
  summ_cell_t *sent = col->cell_top;
  summ_cell_t *cell = sent;

  assert( col->cell_bot->next_col == col->cell_top );

  print_col_header( col );
  do {
    print_cell( cell );
    cell = cell->next_col;
  } while ( cell != sent );
  printf("\n");
}

static void print_matrix_via_cols( summ_matrix_t *sm ) 
{
  /* 
   * Note that this prints sideways; you can cock your head 90 degrees
   * to your left to read its output.
   *
   * So if the rows print as (roughly):
   *   r1    A   B       D
   *   r2        F   G   H
   *   r3    I   J
   *   r4        K   L   M
   * 
   * then the cols print as (roughly):
   *   c4    D   H       M
   *   c3        G       L
   *   c2    B   F   J   K
   *   c1    A       I
   */

  int i;
  int num_cols = DATA(sm)->num_cols;

  /* traverse columns from right to left (see above pic) */
  for (i = num_cols-1; i >= 0; i--) {
    print_col( DATA(sm)->cols[i] );
  }
  printf("\n");
}

static void print_matrix( char *ctxt, summ_matrix_t *sm ) 
{
  printf("%s\n", ctxt);
  print_matrix_via_rows( sm );
  print_matrix_via_cols( sm );
}

static summ_row_t* alloc_row(int source_gno)
{
  summ_row_t *row;
  summ_cell_t *sntl;

  sntl = make_sentinel_cell( source_gno, -1 );
  row = (summ_row_t*)must_malloc( sizeof( summ_row_t ));
  row->cell_lft = sntl;
  row->cell_rgt = sntl;
  row->source_gno = source_gno;
  return row;
}

static summ_row_t** alloc_rows(int num_rows) {
  summ_row_t** rows;
  int i;
  int gno = 0;
  rows = (summ_row_t**)must_malloc( num_rows*sizeof( summ_row_t* ));
  for(i = 0; i < num_rows; i++) {
    rows[i] = alloc_row(gno);
    gno++;
  }
  return rows;
}

static summ_col_t* alloc_col( int target_gno )
{
  summ_col_t *col;
  summ_cell_t *sntl;
  sntl = make_sentinel_cell( -1, target_gno );
  col = (summ_col_t*)must_malloc( sizeof( summ_col_t ));
  col->cell_top = sntl;
  col->cell_bot = sntl;
  col->summarize_word_count = 0;
  col->writebarr_word_count = 0;
  col->collector_word_count = 0;
  col->summacopy_word_count = 0;
  col->target_gno = target_gno;
  col->overly_popular = FALSE;
  col->construction_inprogress = FALSE;
  col->construction_complete = FALSE;
  col->construction_predates_snapshot = FALSE;
  col->sum_mutator = NULL; /* Pooled remsets */
  assert( col->cell_bot->next_col == col->cell_top );
  return col;
}

static summ_col_t** alloc_cols(int num_cols) {
  summ_col_t** cols;
  int i;
  int gno = 0;
  cols = (summ_col_t**)must_malloc( num_cols*sizeof( summ_col_t* ));
  for(i = 0; i < num_cols; i++) {
    cols[i] = alloc_col(gno);
    assert( cols[i] != NULL );
    assert( cols[i]->cell_top != NULL );
    gno++;
  }
  return cols;
}

/* Functions relevant to summary construction: 
 * protocol is:
 * 
 * for each region i
 *   prior_scan(i)
 *   for each object A in i
 *     object_for_scan(A)
 *     for each ref in A
 *        entry_from_scan(gen_of(ref))
 *   after_scan(i)
 */
void sm_prior_scan( summ_matrix_t *summ, int source_gno );

void sm_object_for_scan( summ_matrix_t *summ, word source_obj );

void sm_entry_from_scan( summ_matrix_t *summ, int target_gno );

void sm_after_scan( summ_matrix_t *summ, int source_gno );

static void my_prepare_cols( summ_matrix_t *summ, int col_gno, int col_gno_lim ) 
{
  int i;
  DATA(summ)->col_inprogress_first = col_gno;
  DATA(summ)->col_inprogress_lim = col_gno_lim;

  for( i = col_gno; i < col_gno_lim; i++ ) {
    assert( DATA(summ)->cols[i]->summarize_word_count == 0 );
    assert( DATA(summ)->cols[i]->writebarr_word_count == 0 );
    assert( DATA(summ)->cols[i]->construction_inprogress == FALSE );
    assert( DATA(summ)->cols[i]->construction_complete == FALSE );
    assert( DATA(summ)->cols[i]->overly_popular == FALSE );

    /* We do *not* construct cells at this point; we do that on demand, 
     * as additions to the matrix are needed.
     */
  }
}

/* modifies: *west, *east
 * effects: 1. If row has cell w/ tgt_gno, returns cell; o/w returns
 * NULL, and, 2. *wst, *est get tgt_gno's neighbor(s) (where the 
 * sentinel is a neighbor candidate).
 */
static summ_cell_t *scan_row_for_cell( summ_row_t *row, int tgt_gno, 
                                summ_cell_t **wst, summ_cell_t **est )
{
  summ_cell_t *start = row->cell_lft; /* sentinel */
  summ_cell_t *end = row->cell_rgt;
  summ_cell_t *prev_start = NULL;

  assert( start->prev_row == end ); /* cyclic */
  assert( end->next_row == start );
  assert( tgt_gno > 0 );

  assert( start != NULL );
  assert(   end != NULL );

  assert( start->target_gno == -1 );

  /* Four disjoint cases: 
   * 1. row is completely empty
   * 2. cell with tgt_gno occurs in row
   * 3. all elements in row are before tgt_gno
   * 4. tgt_gno not in row, and some gno > tgt_gno in row.
   */

  if (start == end) {                     /* Case 1: sentinels */
    assert( end->target_gno == -1 );
    *wst = start;
    *est = end;
    return NULL;
  }

  end = start; /* real termination condition is when we've cycled back. */

  do {
    if (0) {
      printf("  scan_row( row:%d, tgt_gno:%d, &w, &e ): loop\n", 
             row->source_gno, tgt_gno );
      print_cell( start );
    }
    assert( start->target_gno <= tgt_gno );

    if ( start->target_gno == tgt_gno ) { /* Case 2: found it */
      *wst = prev_start;
      *est = start->next_row;
      return start;
    }

    /* continue search */
    prev_start = start;
    start = start->next_row;

    assert( prev_start->target_gno < tgt_gno );
  } while ( start != end && start->target_gno <= tgt_gno );

  /* left of || is case 3; right of || is case 4. */
  assert( start->target_gno == -1 || start->target_gno > tgt_gno );
  assert( prev_start->target_gno < tgt_gno );

  /* Both cases 3 and 4 can be handled in the same manner. */
  *wst = prev_start;
  *est = start;
  return NULL;
}


/* modifies: *nth, *sth
 * effects: 1. If col has cell w/ tgt_gno, returns cell; o/w returns
 * NULL, and, 2. *nth, *sth get tgt_gno's neighbor(s) (where the 
 * sentinel is a neighbor candidate).
 */
static summ_cell_t *scan_col_for_cell( summ_col_t *col, int src_gno,
                                summ_cell_t **nth, summ_cell_t **sth )
{
  summ_cell_t *start = col->cell_top; /* sentinel */
  summ_cell_t *end = col->cell_bot;
  summ_cell_t *prev_start = NULL;

  assert( end->next_col == start ); /* cyclic */
  assert( start->prev_col == end );
  assert( src_gno > 0 );

  assert( start != NULL );
  assert(   end != NULL );

  assert( start->source_gno == -1 );

  /* Four disjoint cases: 
   * 1. col is completely empty
   * 2. cell with src_gno occurs in col.
   * 3. all elements in col are before src_gno
   * 4. src_gno not in col, and some gno > src_gno in col.
   */

  if (start == end) {                     /* Case 1: sentinels */
    assert( end->source_gno == -1 );
    *nth = start;
    *sth = end;
    return NULL;
  }

  end = start; /* real termination condition is when we've cycled back. */

  do {
    assert( start->source_gno <= src_gno );

    if ( start->source_gno == src_gno ) { /* Case 2: found it */
      *nth = prev_start;
      *sth = start->next_col;
      return start;
    }

    /* continue search */
    prev_start = start;
    start = start->next_col;

    assert( prev_start->source_gno < src_gno );
  } while ( start != end && start->source_gno <= src_gno );

  /* left of || is case 3; right of || is case 4. */
  assert( start->source_gno == -1 || start->source_gno > src_gno );
  assert( prev_start->source_gno < src_gno );

  /* Both cases 3 and 4 can be handled in the same manner. */
  *nth = prev_start;
  *sth = start;
  return NULL;
}

static void  create_refactored_from_memmgr( summ_matrix_t *sm,
                                            int popularity_limit )
{
  DATA(sm)->summaries_count = 0;

  /* data->summaries->region_count = data->region_count; */
  DATA(sm)->popularity_limit = popularity_limit;

  int len = sm->collector->gno_count+1;
  int i;
  DATA(sm)->summaries_count = len;
  DATA(sm)->nursery_locset = create_locset( 0, 0 );
}

static void check_rep( summ_matrix_t *summ );
static void check_rep_1( summ_matrix_t *summ ) {
#if (CHECK_REP_DEGREE >= 1)
  check_rep( summ );
#endif
}
static void check_rep_2( summ_matrix_t *summ ) {
#if (CHECK_REP_DEGREE >= 2)
  check_rep( summ );
#endif
}
static void check_rep_3( summ_matrix_t *summ ) {
#if (CHECK_REP_DEGREE >= 3)
  check_rep( summ );
#endif
}


static void delayed_check_rep( summ_matrix_t *summ )
{
  static int call_count = 0;
  static const int start_on = 500000000;
  if (call_count < start_on) {
    call_count++;
  } else {
    check_rep( summ );
  }
}

static void sm_build_summaries_partial( summ_matrix_t *summ, 
                                        int rgn_next,
                                        int ne_rgn_count,
                                        bool about_to_major );
static void sm_build_summaries_iteration_complete( summ_matrix_t *summ, 
                                                   int ne_rgn_count );
static void sm_build_remset_summaries( summ_matrix_t *summ,
                                       int ne_rgn_count,
                                       int rgn_next,
                                       bool about_to_major );
static void advance_to_next_summary_set( summ_matrix_t *summ,
                                         int rgn_next, 
                                         int ne_rgn_count,
                                         bool about_to_major,
                                         int dA );

EXPORT summ_matrix_t *
create_summ_matrix( gc_t *gc, int first_gno, int initial_num_rgns, 
                    double c, double g, double p, int popularity_limit, 
                    bool about_to_major, int rgn_next )
{
  summ_matrix_t *sm; 
  summ_matrix_data_t *data;
  /* int num_cols = first_gno + initial_num_rgns; */
  int num_cols = gc->gno_count;
  int num_under_construction = (int)(initial_num_rgns * c);
  int num_rows = gc->gno_count;

  dbmsg("  create_summ_matrix( gc, %d, initial_num_rgns: %d,"
        " %f, %f, pop_limit: %d, num_cols: %d )",
        first_gno, initial_num_rgns, c, p, popularity_limit, num_cols );

  assert( 0.0 < c && c <= 1.0 );
  assert( p >= 2.0 );
  assert( popularity_limit > 0 );

  sm = (summ_matrix_t*)must_malloc( sizeof( summ_matrix_t ));
  data = (summ_matrix_data_t*)must_malloc( sizeof( summ_matrix_data_t ));

  data->coverage = c;
  data->goal = g;
  data->p = p;
  data->entries_per_objs_pool_segment = DEFAULT_OBJS_POOL_SIZE;
  data->entries_per_locs_pool_segment = DEFAULT_LOCS_POOL_SIZE;
  data->num_cols = num_cols;
  data->num_rows = num_rows;

  data->rows = alloc_rows( first_gno + num_rows );
  data->cols = alloc_cols( first_gno + num_cols );

  data->col_complete_first = -1;
  data->col_complete_lim = -1;

  data->col_inprogress_first = -1;
  data->col_inprogress_lim = -1;

  data->row_cache.last_cell_valid = FALSE;

  data->cycle_count = 0;
  data->pass_count = 0;
  data->curr_pass_units_count = 0;

  sm->collector = gc;
  sm->data = data;

  my_prepare_cols( sm, first_gno, first_gno + num_under_construction );

  create_refactored_from_memmgr( sm, popularity_limit );

  {
    int goal = (int)ceil(((double)initial_num_rgns) * g);
    int curr = (int)ceil(((double)initial_num_rgns) * c);
    int gno;

    DATA(sm)->summarizing.goal = goal;
    DATA(sm)->summarizing.complete = FALSE;
    DATA(sm)->summarizing.cursor = gc->gno_count;
    DATA(sm)->summarizing.num = gc->gno_count;
  }

  sm_build_remset_summaries( sm, initial_num_rgns, rgn_next,
                             about_to_major );

  check_rep_1(sm);

  return sm;
}

static void sm_expand_summary_gnos( summ_matrix_t *summ, int fresh_gno );
EXPORT void sm_expand_gnos( summ_matrix_t *summ, int fresh_gno )
{
  summ_row_t *fresh_row;
  summ_col_t *fresh_col;
  int new_num_rows, new_num_cols;
  summ_row_t** new_rows;
  summ_col_t** new_cols;
  int i;
  summ_cell_t *cell, *end;

  check_rep_3( summ );

  fresh_row = alloc_row( fresh_gno );
  fresh_col = alloc_col( fresh_gno );

  new_num_rows = DATA(summ)->num_rows + 1;

  assert( fresh_gno <= DATA(summ)->num_rows );

  {
    new_rows = (summ_row_t**)must_malloc( new_num_rows*sizeof( summ_row_t* ));

    for ( i=0; i<fresh_gno; i++ ) {
      new_rows[ i ] = DATA(summ)->rows[ i ];
    }
    new_rows[ fresh_gno ] = fresh_row;
    for ( i=fresh_gno+1; i<new_num_rows; i++ ) {
      DATA(summ)->rows[ i-1 ]->source_gno = i;
      cell = DATA(summ)->rows[ i-1 ]->cell_lft;
      end = cell;
      do {
        cell->source_gno = i;
        cell = cell->next_row;
      } while (cell != end);
      new_rows[ i ] = DATA(summ)->rows[ i-1 ];
    }
  }

  {
    assert( fresh_gno > 0 );
    if (fresh_gno <= DATA(summ)->num_cols ) {
      new_num_cols = DATA(summ)->num_cols + 1;

      new_cols = (summ_col_t**)must_malloc( new_num_cols*sizeof( summ_col_t* ));

      for ( i=0; i<fresh_gno; i++ ) {
        new_cols[ i ] = DATA(summ)->cols[ i ];
      }

      new_cols[ fresh_gno ] = fresh_col;
      for ( i=fresh_gno+1; i<new_num_cols; i++ ) {
        DATA(summ)->cols[ i-1 ]->target_gno = i;
        cell = DATA(summ)->cols[ i-1 ]->cell_top;
        end = cell;
        do {
          cell->target_gno = i;
          cell = cell->next_col;
        } while (cell != end);
        new_cols[ i ] = DATA(summ)->cols[ i-1 ];
      }
    } else {
      /* meet the new boss, same as the old boss */
      new_num_cols = DATA(summ)->num_cols;
      new_cols = DATA(summ)->cols;
    }

    for( i=0; i<new_num_cols; i++ ) {
      assert( new_cols[i] != NULL );
      assert( new_cols[i]->cell_top != NULL );
      assert( new_cols[i]->cell_bot->next_col == new_cols[i]->cell_top );
    }
  }

  sm_expand_summary_gnos( summ, fresh_gno );

  DATA(summ)->num_rows = new_num_rows;
  DATA(summ)->num_cols = new_num_cols;
  DATA(summ)->rows = new_rows;
  DATA(summ)->cols = new_cols;

  check_rep_3( summ );
}

EXPORT void sm_prepare_cols( summ_matrix_t *summ, int col_gno, int col_gno_lim )
{
  check_rep_1( summ );

  my_prepare_cols( summ, col_gno, col_gno_lim );

  check_rep_1( summ );
}

EXPORT void sm_dispose_cols( summ_matrix_t *summ, int col_gno, int col_gno_lim )
{
  check_rep_1( summ );
  assert(FALSE);
  check_rep_1( summ );
}

static bool prev_summarized_p( summ_matrix_t *summ, int rgn );
static void console_printgset( char *prefix, gset_t g );

static void sm_ensure_available( summ_matrix_t *summ, int gno,
                                 int ne_rgn_count, bool about_to_major, int dA );

static void setup_next_wave( summ_matrix_t *summ, int rgn_next, 
                             int ne_rgn_count, bool about_to_major, int dA );

#define quotient2( x, y ) (((x) == 0) ? 0 : (((x)+(y)-1)/(y)))

EXPORT bool sm_progress_would_no_op(  summ_matrix_t *summ, int ne_rgn_count ) 
{
  /* (must be kept in sync with sm_construction_progress below) */

  check_rep_1( summ );

  if ( DATA(summ)->summarizing.waiting ) {
    int goal_budget = calc_goal( summ, ne_rgn_count );
    int max_pop = quotient2( ne_rgn_count, DATA(summ)->p );
    int usable = 
      region_group_count( region_group_wait_nosum ) +
      region_group_count( region_group_wait_w_sum );
    int filled =
      region_group_count( region_group_filled );

    if ( usable - max_pop > goal_budget ) {
      return TRUE;
    } else if (filled < goal_budget) {
      return TRUE;
    } else {
      return FALSE;
    }
  }  else {
    if (! DATA(summ)->summarizing.complete) {
      return FALSE;
    } else if ( region_group_count( region_group_wait_w_sum ) == 0) {
      return FALSE;
    } else {
      return TRUE;
    }
  }
}


EXPORT bool sm_construction_progress( summ_matrix_t *summ, 
                                      int* word_countdown,
                                      int* object_countdown,
                                      int rgn_next,
                                      int ne_rgn_count,
                                      bool about_to_major,
                                      int alloc_per_majgc )
{
  /* (must be kept in sync with sm_progress_would_no_op above) */
  int start, coverage;

  bool completed_cycle = FALSE;

  dbmsg("sm_construction_progress"
        "( summ, rgn_next=%d, ne_rgn_count=%d, about_to_major=%s, dA=%d );",
        rgn_next, ne_rgn_count, about_to_major?"TRUE":"FALSE", alloc_per_majgc );
  dbmsg( "sm_construction_progress: "
              "goal: %d complete: %s cursor: %d num: %d",
              DATA(summ)->summarizing.goal,
              DATA(summ)->summarizing.complete ? "TRUE":"FALSE",
              DATA(summ)->summarizing.cursor, 
              DATA(summ)->summarizing.num );

  check_rep_1( summ );

  assert( DATA(summ)->summarizing.waiting || 
          (DATA(summ)->summarizing.num > 0) );

  if ( DATA(summ)->summarizing.waiting ) {
    int goal_budget = calc_goal( summ, ne_rgn_count );
    int max_pop = quotient2( ne_rgn_count, DATA(summ)->p );
    int usable = 
      region_group_count( region_group_wait_w_sum ) +
      region_group_count( region_group_wait_nosum );
    int filled  =
      region_group_count( region_group_filled );

    if ( usable-max_pop > goal_budget ) {
      /* continue waiting to setup next wave */
      return FALSE;
    } else if (filled < goal_budget) {
      /* continue allocating more storage before setup of next wave */
      return FALSE;
    } else {
      DATA(summ)->summarizing.waiting = FALSE;
      setup_next_wave( summ, rgn_next, ne_rgn_count, about_to_major, 
                       alloc_per_majgc );
    }
  }

  assert( (region_group_count(region_group_summzing) > 0) 
          || (region_group_count(region_group_filled) == 0));

  if (! DATA(summ)->summarizing.complete) {
    bool shall_we_progress;

    assert2( ! DATA(summ)->summarizing.waiting );
    assert2( DATA(summ)->summarizing.num > 0 );

    if (! about_to_major ) {
      shall_we_progress = TRUE; /* yes, lets! */
    } else {
      int fuel, capability, required;

      fuel =
        region_group_count( region_group_wait_nosum ) +
        region_group_count( region_group_wait_w_sum );
      capability = DATA(summ)->summarizing.num * fuel;
      required = DATA(summ)->summarizing.cursor;

#if CONSERVATIVE_REGION_COUNT
      assert( capability >= required );
#endif

      /* the major collection itself will use one unit of fuel. */
      capability = DATA(summ)->summarizing.num * (fuel - 1);

      /* XXX Hmm these calculations are now bogus because the required
       * value is too conservative; doesnt filter out empty remembered
       * sets in the remaining remsets to traverse.  What to do? */
      if ( capability >= required ) {
        /* no need to progress further; we're on budget. */
        shall_we_progress = FALSE;

        /* XXX but should we be eager beavers?  Perhaps invoking
         * sm_build_summaries_partial_n with a smaller count than
         * summarizing.num?
         * 
         * In any case, to bring back behavior from circa SVN revision
         * 6110, set shall_we_progress to TRUE here. */

      } else {
        consolemsg("summ_matrix.c: had to step summarization "
                   "during major gc pause to meet budget");
        shall_we_progress = TRUE;
      }
    }

    if ( shall_we_progress ) {
      /* make progress on next wave. */
      assert( ! DATA(summ)->summarizing.complete );
      sm_build_summaries_partial( summ, rgn_next, ne_rgn_count, about_to_major );
      if (DATA(summ)->summarizing.complete) {
        completed_cycle = TRUE;
      }
    }

  } else {
    /* next wave complete; shift if appropriate. */
    completed_cycle = TRUE;
    if ( region_group_count( region_group_wait_w_sum ) == 0 ) {
      sm_ensure_available( summ, rgn_next, ne_rgn_count, about_to_major, 
                           alloc_per_majgc );
    }
  }

  if (about_to_major 
      && ( gc_region_group_for_gno( summ->collector, rgn_next )
           != region_group_wait_nosum )) {
    sm_ensure_available( summ, rgn_next, ne_rgn_count, about_to_major,
                         alloc_per_majgc );

    /* rgn_next is part of previous summarization wave */
    assert( prev_summarized_p( summ, rgn_next ));
  }

  check_rep_1( summ );

  return completed_cycle;
}

EXPORT void sm_enumerate_row( summ_matrix_t *summ,
                       int row_gno, 
                       bool (*scanner)(word loc, void *data),
                       void *data )
{
  check_rep_1( summ );
  assert(FALSE);
  check_rep_1( summ );
}

EXPORT void sm_enumerate_col( summ_matrix_t *summ, 
                       int col_gno, 
                       bool (*scanner)(word loc, void *data),
                       void *data )
{
  check_rep_1( summ );
  assert(FALSE);
  check_rep_1( summ );
}


EXPORT void sm_add_entry( summ_matrix_t *summ, word source_obj, int target_gno )
{
  check_rep_3( summ );
  assert(FALSE);
  check_rep_3( summ );
}

EXPORT void sm_next_summary( summ_matrix_t *summ, summary_t *column ) 
{
  check_rep_1( summ );
  assert(FALSE);
  check_rep_1( summ );
}

/* Functions below are for use when this structure is being used in a
 * concurrent (ie multi-threaded) regional collector. */

EXPORT void sm_add_entry_concurrent( summ_matrix_t *summ, 
                              word source_obj, 
                              int target_gno )
{
  check_rep_3( summ );
  assert(FALSE);
  check_rep_3( summ );
}


EXPORT void sm_construction_concurrent( summ_matrix_t *summ,
                                 int grain_scan_words,
                                 int grain_scan_objects )
{
  check_rep_3( summ );
  assert(FALSE);
  check_rep_3( summ );
}


EXPORT void sm_interrupt_construction( summ_matrix_t *summ )
{
  check_rep_3( summ );
  assert(FALSE);
  check_rep_3( summ );
}

EXPORT bool sm_is_rgn_summary_avail( summ_matrix_t *summ, int gno ) 
{
  region_group_t grp;
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  return (grp == region_group_wait_w_sum);
}
EXPORT bool sm_is_rgn_summarized( summ_matrix_t *summ, int gno ) 
{
  region_group_t grp; 
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  return (grp == region_group_wait_w_sum || grp == region_group_popular);
}
EXPORT bool sm_will_rgn_be_summarized_next( summ_matrix_t *summ, int gno )
{
  region_group_t grp;
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  return (grp == region_group_summzing || grp == region_group_popular);
}
EXPORT bool sm_is_rgn_summarized_next( summ_matrix_t *summ, int gno ) 
{
  bool rtn;
  region_group_t grp;
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  check_rep_1( summ );
  rtn = DATA(summ)->summarizing.complete &&
    (grp == region_group_summzing || grp == region_group_popular);
  return rtn;
}
EXPORT bool sm_is_rgn_summary_avail_next( summ_matrix_t *summ, int gno )
{
  bool rtn;
  region_group_t grp;
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  check_rep_1( summ );
  rtn = DATA(summ)->summarizing.complete &&
    (grp == region_group_summzing);
  return rtn;
}
EXPORT bool sm_is_rgn_summary_over_pop( summ_matrix_t *summ, int gno )
{
  region_group_t grp;
  check_rep_1( summ );
  grp = gc_region_group_for_gno( summ->collector, gno );
  return (grp == region_group_popular);
}

EXPORT bool sm_has_valid_summaries( summ_matrix_t *summ )
{
  check_rep_1( summ );
  assert( 0 );
  return 0;
}
EXPORT void sm_push_nursery_summary( summ_matrix_t *summ, smircy_context_t *smircy )
{
  check_rep_1( summ );
  smircy_push_locset( smircy, DATA(summ)->nursery_locset );
  check_rep_1( summ );
}
EXPORT void sm_clear_nursery_summary( summ_matrix_t *summ )
{
  check_rep_1( summ );
  dbmsg("sm_clear_nursery_summary( summ )");
  ls_clear( DATA(summ)->nursery_locset );
  check_rep_1( summ );
}

/* below refactored from memmgr.c */

typedef struct remset_summary_data remset_summary_data_t;
struct remset_summary_data {
  summ_matrix_t *summ;
  int count_objects_visited;
  int count_objects_added;
  int words_added;
};

static struct {
  locset_t **elem;
  int len;
} locset_pool = { NULL, 0 };

static locset_t* grab_from_locset_pool(void) 
{
  { int i; 
    for (i = 0; i < locset_pool.len; i++) {
      if (locset_pool.elem[i] != NULL) {
        locset_t *rtn;
        rtn = locset_pool.elem[i];
        locset_pool.elem[i] = NULL;
        assert2(rtn->live == 0);
        return rtn;
      }
    }
  }
  /* if we get here, then all remsets are in use and we need to expand
   * the pool. */
  {
    int newlen = locset_pool.len + 1;
    locset_t **elem = (locset_t**)must_malloc(newlen*sizeof(locset_t*));
    free(locset_pool.elem);
    locset_pool.elem = elem;
    locset_pool.len = newlen;
    /* we do not store the new remset in the pool yet; that will
     * happen when it is freed. */
    memset(elem, 0, newlen*sizeof(locset_t*));
    return create_locset( 0, 0 );
  }
}

static void return_to_locset_pool( locset_t *ls ) 
{
  int i;
  assert2(ls->live == 0);
  for (i = 0; i < locset_pool.len; i++) {
    if (locset_pool.elem[i] == NULL) {
      locset_pool.elem[i] = ls;
      return;
    }
  }
  assertmsg("no NULL entries in a pool of length %d", locset_pool.len);
  assert(0); /* (should never get here) */
}

static int col_words( summ_col_t *c )
{
  return c->summarize_word_count + c->writebarr_word_count 
    + c->collector_word_count 
    /*+ c->summarize_word_count*/ /* This was always bogus */
    + c->summacopy_word_count /* willnot be (as) bogus after I fix stuff */;
}
static void col_reset_words( summ_col_t *c ) 
{
  c->summarize_word_count = 0;
  c->writebarr_word_count = 0;
  c->collector_word_count = 0;
  c->summacopy_word_count = 0;
}

typedef enum { incr_ctxt_sm, incr_ctxt_wb, incr_ctxt_gc } incr_ctxt_t;

static void col_incr_once_sm( summ_col_t *c, int dwords ) 
{
  c->summarize_word_count += 1;
}
static void col_incr_once_wb( summ_col_t *c, int dwords ) 
{
  c->writebarr_word_count += 1;
}
static void col_incr_once_gc( summ_col_t *c, int dwords ) 
{
  c->collector_word_count += 1;
}
static void col_incr_twice_sm( summ_col_t *c, int dwords ) 
{
  c->summarize_word_count += 2;
}
static void col_incr_twice_wb( summ_col_t *c, int dwords ) 
{
  c->writebarr_word_count += 2;
}
static void col_incr_twice_gc( summ_col_t *c, int dwords ) 
{
  c->collector_word_count += 2;
}
static void col_incr_words_sm( summ_col_t *c, int dwords ) 
{
  c->summarize_word_count += dwords;
}
static void col_incr_words_wb( summ_col_t *c, int dwords ) 
{
  c->writebarr_word_count += dwords;
}
static void col_incr_words_gc( summ_col_t *c, int dwords ) 
{
  c->collector_word_count += dwords;
}

/* Returns cell of summ[tgt_gno] that holds objects from src_gno rgn,
 * or NULL if tgt_gno is not being summarized right now.
 * 
 * NOTE: each invocation of this fcn with a *different* src_gno from the
 * preceding invocation is expensive.  The common case is intended to be 
 * a series of calls with the same src_gno; switches to other 
 * src_gno's occur but should be rare events.
 */
static summ_cell_t* summ_cell( summ_matrix_t *summ, int src_gno, int tgt_gno )
{
  /* FIXME: simple unscalable implementation(s) first */
  /* [ going to have to define the allocation protocol much more 
   *   carefully to scale all this up.  :( ] */
  summ_cell_t *n, *s, *e, *w;
  summ_row_t *row;
  summ_col_t *col;
  summ_cell_t *row_cell, *col_cell;

  assert( src_gno < DATA(summ)->num_rows );
  assert( tgt_gno < DATA(summ)->num_cols );

  if ( DATA(summ)->row_cache.last_src_gno == src_gno &&
       DATA(summ)->row_cache.last_tgt_gno == tgt_gno &&
       DATA(summ)->row_cache.last_cell_valid ) {
    return DATA(summ)->row_cache.last_cell;
  }

  row = DATA(summ)->rows[src_gno];
  col = DATA(summ)->cols[tgt_gno];
  assert( row != NULL );
  assert( col != NULL );
  row_cell = scan_row_for_cell( row, tgt_gno, &w, &e );
  col_cell = scan_col_for_cell( col, src_gno, &n, &s );

  if (row_cell != col_cell ) {
    assertmsg("  %s( summ, src: %d, tgt: %d ) num_rows: %d num_cols: %d"
               " => via row: %d, via col: %d", 
               "summ_cell", src_gno, tgt_gno, 
               DATA(summ)->num_rows, DATA(summ)->num_cols,
               (row_cell==NULL)?0:row_cell->dbg_id,
               (col_cell==NULL)?0:col_cell->dbg_id );
  }

  assert( row_cell == col_cell );

  if (row_cell == NULL) {
    dbmsg("row_cell null; w:%d e:%d n:%d s:%d", 
          w->dbg_id, e->dbg_id, n->dbg_id, s->dbg_id );
    assert( e->prev_row == w );
    assert( w->next_row == e );
    assert( s->prev_col == n );
    assert( n->next_col == s );
    assert( s != NULL );
    row_cell = make_cell( src_gno, tgt_gno );
    e->prev_row = row_cell;
    row_cell->next_row = e;
    w->next_row = row_cell;
    row_cell->prev_row = w;
    if ( row->cell_rgt == w ) {
      row->cell_rgt = row_cell;
    }
    s->prev_col = row_cell;
    row_cell->next_col = s;
    n->next_col = row_cell;
    row_cell->prev_col = n;
    if ( col->cell_bot == n ) {
      col->cell_bot = row_cell;
    }
    assert( col->cell_bot->next_col == col->cell_top );
  }

  DATA(summ)->row_cache.last_src_gno = src_gno;
  DATA(summ)->row_cache.last_tgt_gno = tgt_gno;
  DATA(summ)->row_cache.last_cell = row_cell;
  DATA(summ)->row_cache.last_cell_valid = TRUE;
  return row_cell;
}

static word objpool_last_entry( objs_pool_t *objects ) 
{
  while (objects != NULL) {
    if ( objects->top > objects->bot ) {
      return objects->top[-1];
    }
    objects = objects->next;
  }
  return 0x0;
}

static void assert_not_present_in_objpool( summ_matrix_t *summ,
                                           objs_pool_t *objects,
                                           word ptr )
{
  objs_pool_t *objs = objects;
  int i;
  i = 0;
  while (objs != NULL) {
    word *p = objs->bot;
    word *top = objs->top;
    while ( p < top ) {
      if (*p != 0x0) {
        assert( ptr != *p );
      }
      p++;
      i++;
    }
    objs = objs->next;
  }
}

/* requires: ptr not in objects
 * modifies: objects
 * effects: returns a pool p = objects u { ptr }
 */
static objs_pool_t *pool_enq_obj( summ_matrix_t *summ, objs_pool_t *objects, word ptr )
{
  int entries;

#if POOL_ENQUEUE_ASSERTS_UNIQ_ENQ
  assert_not_present_in_objpool( summ, objects, ptr ); /* (expensive) */
#endif

  entries = DATA(summ)->entries_per_objs_pool_segment;
  if ( (objects == NULL) || (objects->top == objects->lim) ) {
    objs_pool_t *p = alloc_objpool_segment( entries );
    p->next = objects;
    objects = p;
  }
  *objects->top = ptr;
  objects->top += 1;
  return objects;
}

/* requires: loc not in locations
 * modifies: locations
 * effects: returns a pool p = locations u { loc }
 */
static locs_pool_t *pool_enq_loc( summ_matrix_t *summ, 
                                  locs_pool_t *locations, loc_t loc )
{
  int entries;

#if POOL_ENQUEUE_ASSERTS_UNIQ_ENQ
  assert_not_present_in_locpool( summ, locations, loc ); /* (expensive) */
#endif

  entries = DATA(summ)->entries_per_locs_pool_segment;
  if ( (locations == NULL) || (locations->top == locations->lim) ) {
    locs_pool_t *p = alloc_locpool_segment( entries );
    p->next   = locations;
    locations = p;
  }
  *locations->top = loc;
  locations->top += 1;
  return locations;
}

static word cell_last_entry( summ_matrix_t *summ, summ_cell_t *cell )
{
  return objpool_last_entry( cell->objects );
}

static void cell_enq_obj( summ_matrix_t *summ, summ_cell_t *cell, word ptr )
{
  /* assert2( ptr occurs nowhere in summ[cell->tgt].row[cell->src] */
  cell->objects = pool_enq_obj( summ, cell->objects, ptr );
}

static void cell_enq_loc( summ_matrix_t *summ, summ_cell_t *cell, loc_t loc )
{
  cell->locations = pool_enq_loc( summ, cell->locations, loc );
}

static void clear_col_cells( summ_matrix_t *summ, int col_idx );
static void clear_col_mutator_rs( summ_matrix_t *summ, int col_idx );

static bool col_contains_loc( summ_matrix_t *summ, summ_col_t *col, loc_t loc )
{
  summ_cell_t *cell, *sent;
  cell = col->cell_top;
  sent = cell;
  do {
    locs_pool_t *locs;
    for( locs = cell->locations; locs != NULL; locs = locs->next ) {
      loc_t *lptr;
      for( lptr = locs->bot; lptr < locs->top; lptr++ ) {
        loc_t l = *lptr;
        if (l.obj == loc.obj && l.offset == loc.offset)
          return TRUE;
      }
    }
    cell = cell->next_col;
  } while (cell != sent);

  return FALSE;
}
static bool col_contains_ptr( summ_matrix_t *summ, summ_col_t *col, word ptr )
{
  summ_cell_t *cell, *sent;
  cell = col->cell_top;
  sent = cell;
  do {
    objs_pool_t *objs;
    for( objs = cell->objects; objs != NULL; objs = objs->next ) {
      word *wptr;
      for( wptr = objs->bot; wptr < objs->top; wptr++ ) {
        if (*wptr == ptr)
          return TRUE;
      }
    }
    cell = cell->next_col;
  } while (cell != sent);

  return FALSE;
}

static void col_print_ptr( summ_matrix_t *summ, summ_col_t *col )
{
  summ_cell_t *cell, *sent;
  cell = col->cell_top;
  sent = cell;
  do {
    objs_pool_t *objs;
    for( objs = cell->objects; objs != NULL; objs = objs->next ) {
      word *wptr;
      for( wptr = objs->bot; wptr < objs->top; wptr++ ) {
        printf("0x%08x (%d) ", *wptr, gen_of(*wptr));
      }
    }
    cell = cell->next_col;
  } while (cell != sent);

  printf("\n");
  fflush(0);
}

static void oflo_check_word( summ_matrix_t *summ, 
                             int tgno, summ_col_t *col );

static void oflo_check_loc( summ_matrix_t *summ, 
                            int tgno, loc_t loc, summ_col_t *col );

static void incr_size_and_oflo_check( summ_matrix_t *summ, int tgno, word w,
                                      void (*incr_words)( summ_col_t *c, int dw ))
{
  int word_count;
  summ_col_t *col = DATA(summ)->cols[tgno];
  if (tagof(w) == PAIR_TAG) {
    word_count = 2;
  } else {
    word_count = sizefield( *ptrof(w) ) / sizeof(word);
  }

  incr_words( col, word_count );
  oflo_check_word( summ, tgno, col );
} 

static void oflo_check_word( summ_matrix_t *summ, 
                             int tgno, summ_col_t *col )
{
  int pop_limit = DATA(summ)->popularity_limit;
  if (col_words(col) > pop_limit) {
    dbmsg( "cellsumm for rgn %d overflowed: %d max %d",
           tgno, col_words(col), pop_limit );
    col->overly_popular = TRUE;
    { 
      old_heap_t *heap; 
      region_group_t grp;
      heap = gc_heap_for_gno( summ->collector, tgno );
      grp = heap->group; 
      assert( grp == region_group_summzing || grp == region_group_wait_w_sum );
      /* Q: do we want to allow waiting to shift to popular? */
      region_group_enq( heap, grp, region_group_popular );
    }
    clear_col_cells( summ, tgno );
    clear_col_mutator_rs( summ, tgno );
  }
}

static void oflo_check_loc( summ_matrix_t *summ, 
                            int tgno, loc_t loc, summ_col_t *col )
{
  oflo_check_word( summ, tgno, col );
}

/* Let A be summ[tgt_gen].cell[src_gen].objects.
 * requires: (ptr in A) implies (ptr is A's latest entry)
 * modifies: A
 * effects: if ptr in A, do nothing.  Else enqueue ptr in A.
 */
static void add_object_to_sum_array( summ_matrix_t *summ, 
                                     int tgt_gen,
                                     word ptr, 
                                     int src_gen,
                                     void (*col_incr_w)(summ_col_t *c, int dw))
{
  int pop_limit = DATA(summ)->popularity_limit;
  summ_col_t *col = DATA(summ)->cols[tgt_gen];

  if (col_words(col) <= pop_limit) {
    summ_cell_t *cell = summ_cell( summ, src_gen, tgt_gen );
    if (cell == NULL) {
      /* do nothing */
    } else if (cell_last_entry(summ, cell) == ptr) {
      /* do nothing */
    } else {
#if ADD_TO_SUMAR_ASSERTS_UNIQ_ENQ 
        assert( ! col_contains_ptr(summ, col, ptr) );
#endif

      cell_enq_obj( summ, cell, ptr );
      incr_size_and_oflo_check( summ, tgt_gen, ptr, col_incr_w );
    }
  }

  /* XXX */
  /* The interface for this function might not make sense, because it
   * is too expensive (ie >> constant time) to lookup the Cell into
   * which we are inserting ptr.
   *
   * Idea: Maintain the illusion of this interface by keeping a
   * transient table in the summ that maps SZING -> [Maybe Cell].
   * That would require that all invocations on gen_of(ptr) are
   * adjacent, but I actually think that holds for any particular
   * traversal.  (Note that currently there are two traversals that
   * use this callback; the per-remset scan in
   * sm_build_remset_summaries, and the per-tospace scan in
   * sm_points_across_callback.)
   *
   * Secondary Idea: why not make the illusion concrete by adding the
   * table as a parameter to this function?  (Doing so may require
   * generalizing some of the void* data arguments to callbacks to
   * types beyond summ_matrix_t*, but that's okay.)
   */

  /* Adding ptr to the Col's hashset *always* be a sound addition,
   * since there can be overlap between a Col's cells and its hashset.
   */
}

static void incr_once( summ_col_t *col, incr_ctxt_t incr_ctxt )
{
  void (*col_incr_w)(summ_col_t *c, int dw);
  switch (incr_ctxt) {
  case incr_ctxt_sm: col_incr_once_sm(col, 1); break;
  case incr_ctxt_wb: col_incr_once_wb(col, 1); break;
  case incr_ctxt_gc: col_incr_once_gc(col, 1); break;
  default: assert(0);
  }
}

static void add_loc_to_sum_array( summ_matrix_t *summ, 
                                  int tgt_gen, loc_t loc, int src_gen, 
                                  incr_ctxt_t incr_ctxt )
{
  int pop_limit = DATA(summ)->popularity_limit;
  summ_col_t *col = DATA(summ)->cols[tgt_gen];

  assert2( src_gen != tgt_gen );
  assert2( gen_of( loc.obj ) == src_gen );
  assert2( urs_isremembered( summ->collector->the_remset, loc.obj ));

  if (col_words(col) <= pop_limit) {
    summ_cell_t *cell = summ_cell( summ, src_gen, tgt_gen );
    if (cell == NULL) { 
      /* do nothing */
    } else {
#if ADD_TO_SUMAR_ASSERTS_UNIQ_ENQ
        if (col_contains_loc(summ, col, loc)) {
          consolemsg("add_loc_to_sum_array fail 0x%08x[%d] (%3d, %3d) words: %d",
                     loc.obj, loc.offset, src_gen, tgt_gen, col_words(col));
        }
        assert( ! col_contains_loc(summ, col, loc) );
#endif

      cell_enq_loc( summ, cell, loc );
      incr_once( col, incr_ctxt );
      oflo_check_loc( summ, tgt_gen, loc, col );
    }
  }
}

static void add_location_to_mut_rs( summ_matrix_t *summ, int g_rhs, loc_t loc )
{
  locset_t *ls; 
  /* XXX TODO: filter addition to count words and wave-off when
   * summary has become over-populated. */ 
  ls = DATA(summ)->cols[ g_rhs ]->sum_mutator;
  if (ls == NULL) {
    ls = grab_from_locset_pool();
    DATA(summ)->cols[ g_rhs ]->sum_mutator = ls;
  }

  {
    word *slot;
    slot = loc_to_slot(loc);
    if (! is_pair( loc.obj )) {
      ls_add_nonpair( ls, slot );
    } else if (loc.offset == 0) {
      ls_add_paircar( ls, slot );
    } else {
      ls_add_paircdr( ls, slot );
    }
  }
}

static bool region_summarized( summ_matrix_t *summ, int gno );
static bool region_summarizing_goal( summ_matrix_t *summ, int gno );
static bool region_summarizing_curr( summ_matrix_t *summ, int gno );

/* These come from the mutator, so there may be duplicates.
 * XXX
 * Therefore they go in columns' hashsets (represented by remset_t).
 */
EXPORT void sm_add_ssb_elems_to_summary( summ_matrix_t *summ, word *bot, word *top, int g_rhs )
{
  word *p, *q, w, w2;
  int pop_limit;
  summ_col_t *col;

  check_rep_3( summ );

  pop_limit = DATA(summ)->popularity_limit;
  col = DATA(summ)->cols[g_rhs];

  ls_add_elems_funnel( DATA(summ)->nursery_locset, bot, top );

  if ( region_summarized( summ, g_rhs ) || 
       region_summarizing_goal( summ, g_rhs )) {
    p = bot; 
    q = top; 
    while (q > p) {
      q--;
      w2 = *q;
      assert(is_fixnum(w2));
      assert( 1 == ((top - q) % 2));
      assert(q > p);
      q--;
      w = *q;
      assert((is_fixnum(w2)) && (! is_fixnum(w)));
      assert( 0 == ((top - q) % 2));
    }
    p = bot; 
    q = top; 
    assert( 0 == ((top - q) % 2));
    while (q > p) {
      assert( 0 == ((top - q) % 2));
      q--;
      assert( 1 == ((top - q) % 2));
      w = *q;
      if (! is_fixnum(w)) {
        consolemsg("non-fixnum: 0x%08x at q: 0x%08x p: 0x%08x (q-p):%d (top-q):%d", w, q, p, (q-p), (top-q));
      }
      assert( 1 == ((top - q) % 2));
      assert(is_fixnum(w));

      assert( q > p );
      w2 = w;
      q--; 
      assert( 0 == ((top - q) % 2));
      w = *q;

      assert(is_fixnum(w2));
      assert( ! is_fixnum(w) );
      assert( is_ptr(w) );

      if (col_words(col) <= pop_limit) {
        loc_t loc;
        loc = make_loc( w, w2 );
        add_location_to_mut_rs( summ, g_rhs, loc );
        incr_size_and_oflo_check( summ, g_rhs, w, col_incr_once_wb );
      }
      assert( 0 == ((top - q) % 2));
    }
  }

  check_rep_3( summ );
}

static bool scan_object_for_remset_summary( word ptr, void *data )
{
  word *loc = ptrof(ptr);
  word scanned = 0;
  bool do_enqueue = FALSE;
  remset_summary_data_t *remsum = (remset_summary_data_t*)data;
  int mygen = gen_of(ptr); 
#if SUMMARIZE_KILLS_RS_ENTRIES
  bool keep_in_remembered_set = FALSE;
#endif
  /* XXX fixme: the way remset's are scanned, we should not need to reextract this */

  static const bool instrumented = FALSE;

  {
    gc_t *gc = remsum->summ->collector;
    if (gc->smircy != NULL && 
        smircy_in_refinement_stage_p( gc->smircy ) &&
        ! smircy_object_marked_p( gc->smircy, ptr )) {
      return FALSE; /* smircy wants us to remove ptr from the remembered set. */
    }
  }

  if (tagof( ptr ) == PAIR_TAG) {
    /* handle car */
    if (isptr(*loc)) {
      int gen = gen_of(*loc);
      if (instrumented) 
        annoyingmsg("scan_object_for_remset_summary "
                    "pair 0x%08x (%d) car: 0x%08d (%d)", 
                    ptr, gen_of(ptr), *loc, gen);
      if (mygen != gen) {
#if SUMMARIZE_KILLS_RS_ENTRIES
        if (! gc_is_nonmoving( remsum->summ->collector, gen )) {
          keep_in_remembered_set = TRUE;
        }
#endif
        if ( gc_region_group_for_gno( remsum->summ->collector, gen )
             == region_group_summzing ) {
          do_enqueue = TRUE;
          { loc_t loc;
            loc = make_loc( ptr, 0 );
            add_loc_to_sum_array( remsum->summ, 
                                  gen, loc, mygen, incr_ctxt_sm );
          }
        }
      }
    }
    ++loc;
    /* handle cdr */
    if (isptr(*loc)) {
      int gen = gen_of(*loc);
      assert( gen >= 0 );
      if (instrumented) 
        annoyingmsg("scan_object_for_remset_summary "
                    "pair 0x%08x (%d) cdr: 0x%08d (%d)", 
                    ptr, gen_of(ptr), *loc, gen);
      if (mygen != gen) {
#if SUMMARIZE_KILLS_RS_ENTRIES
        if (! gc_is_nonmoving( remsum->summ->collector, gen )) {
          keep_in_remembered_set = TRUE;
        }
#endif
        if ( gc_region_group_for_gno( remsum->summ->collector, gen )
             == region_group_summzing ) {
          do_enqueue = TRUE;
          { loc_t loc;
            loc = make_loc( ptr, sizeof(word)); /* offset is in bytes */
            add_loc_to_sum_array( remsum->summ, 
                                  gen, loc, mygen, incr_ctxt_sm );
          }
        }
      }
    }
    scanned = 2;
  } else { /* vector or procedure */
    word words;
    int offset;
    assert( (tagof(ptr) == VEC_TAG) || (tagof(ptr) == PROC_TAG) );
    words = sizefield( *loc ) / 4;
    scanned = words;
    offset = 0;
    while (words--) {
      ++loc;
      offset += sizeof(word);
      if (isptr(*loc)) {
        int gen = gen_of(*loc);
        assert2( gen >= 0 );
        if (instrumented) 
          annoyingmsg("scan_object_for_remset_summary "
                      "vecproc 0x%08x (%d) : 0x%08x (%d)", 
                      ptr, gen_of(ptr), *loc, gen);
        if (mygen != gen) {
#if SUMMARIZE_KILLS_RS_ENTRIES
          if (! gc_is_nonmoving( remsum->summ->collector, gen )) {
            keep_in_remembered_set = TRUE;
          }
#endif
          if ( gc_region_group_for_gno( remsum->summ->collector, gen )
               == region_group_summzing ) {
            loc_t loc;
            do_enqueue = TRUE;
            loc = make_loc( ptr, offset );
            add_loc_to_sum_array( remsum->summ, 
                                  gen, loc, mygen, incr_ctxt_sm );
          }
        }
      }
    }
  }
  
  remsum->count_objects_visited += 1;
  if (do_enqueue) {
    remsum->count_objects_added += 1;
    remsum->words_added += scanned;
  }

 end:
#if SUMMARIZE_KILLS_RS_ENTRIES
  return keep_in_remembered_set;
#else
  return TRUE; /* don't remove entries from the remembered set we are summarizing! */  
#endif
}

static bool prev_summarized_p( summ_matrix_t *summ, int rgn ) 
{
  return region_summarized( summ, rgn );
}

static bool next_summarized_p( summ_matrix_t *summ, int rgn )
{
  return
    (DATA(summ)->summarizing.complete && region_summarizing_goal( summ, rgn ));
}

/* resets (reinitializes) summary state for all in region_group_summzing */
static void sm_build_summaries_setup( summ_matrix_t *summ,
                                      int majors, int ne_rgn_count,
                                      int rgn_next, int about_to_major,
                                      int dA )
{
  int i;
  int num_under_construction;
  int goal;
  int gc_budget; 

  goal = calc_goal( summ, ne_rgn_count );

  /* genset_next does not actually go up to coverage
   * when that would overlap already summarized area. 
   * (and Felix can't get the new logic right...) */
  dbmsg("sm_build_summaries_setup"
        "(summ, majors=%d, ne_rgn_count=%d, rgn_next=%d, about_to_major=%s )"
        " goal:%d",
        majors, ne_rgn_count, rgn_next, about_to_major?"TRUE":"FALSE", goal );

  gc_budget = about_to_major ? majors : (majors+1);

  assert( gc_budget > 0 );

  /* XXX num_under_construction should be bounded by a constant
   * dependant on the sumzbudget, sumzcoverage, and popularity; and
   * the code should be checking that the bound is satisfied. */
  {
    int W, dA_over_R;
#if CONSERVATIVE_REGION_COUNT
    N_over_R = summ->collector->gno_count;
    U = region_group_count( region_group_unfilled );
#endif
    W = region_group_count( region_group_wait_w_sum ) +
      region_group_count( region_group_wait_nosum );
    if (! about_to_major)
      W = W+1;
    dA_over_R = dA;
    num_under_construction = 
      (quotient2( ne_rgn_count, W ) + dA_over_R );
#if 0
    num_under_construction = 
      quotient2( summ->collector->gno_count, gc_budget );
#endif

    dbmsg("sm_build_summaries_setup"
          "( summ, majors=%d, ne_rgn_count=%d, rgn_next=%d, about_to_major=%d, dA=%d )"
          " W=%d dA/R=%d ==> num=%d ",
          majors, ne_rgn_count, rgn_next, about_to_major, dA, 
          W, dA_over_R, num_under_construction );

    assert2( num_under_construction > 0 );
  }


  DATA(summ)->summarizing.goal = goal;
  DATA(summ)->summarizing.waiting = FALSE;
  DATA(summ)->summarizing.complete = FALSE;
  DATA(summ)->summarizing.cursor = summ->collector->gno_count;
  DATA(summ)->summarizing.num = num_under_construction;

  /* Optimistically assume that summarization will succeed for all
   * elems of genset; if one of them overflows, it will be
   * responsibility of scan_object_for_remset_summary to set valid
   * field to FALSE.
   */
  { 
    int i;
    old_heap_t *oh;
    oh = region_group_first_heap( region_group_summzing );
    while (oh != NULL) {
      gno_state_t gno_state;
      i = oh_current_space( oh )->gen_no;
      gno_state = gc_gno_state( summ->collector, i);
      switch ( gno_state ) {

      case gno_state_normal:
        DATA(summ)->cols[i]->overly_popular = FALSE;
        col_reset_words( DATA(summ)->cols[i] );
        /* XXX kill below after shifting to cells rep */
        break;

      case gno_state_popular:
        DATA(summ)->cols[i]->overly_popular = TRUE;
        break;
      default: 
        assert(0);
      }

      oh = region_group_next_heap( oh );
    }
  }
}

static int sm_build_summaries_by_scanning( summ_matrix_t *summ,
                                           int start_remset,
                                           int finis_remset,
                                           remset_summary_data_t *p_remsum )
{
  int i;
  int nontrivial_scans = 0;

  dbmsg( "sm_build_summaries_by_scanning"
              "( summ, start_remset=%d, finis_remset=%d, p_remset );",
              start_remset, finis_remset );

  for ( i = start_remset; i < finis_remset; i++ ) {
    /* enumerating all *rows*; thus this is the pROWduction loop. */
    dbmsg("enum remsets of %d, live %d", i, 
          urs_live_count( summ->collector->the_remset, i ));
    urs_enumerate_gno( summ->collector->the_remset, TRUE, i, 
                       scan_object_for_remset_summary, 
                       (void*) p_remsum );
    
    if ( urs_live_count( summ->collector->the_remset, i ) > 0 ) {
      nontrivial_scans += 1;
    }
  }

  assert(finis_remset >= start_remset);
  DATA(summ)->curr_pass_units_count += (finis_remset - start_remset);

  return nontrivial_scans;
}

static void sm_clear_col( summ_matrix_t *summ, int i );
static void wait_to_setup_next_wave( summ_matrix_t *summ );

static void switch_some_to_summarizing( summ_matrix_t *summ, int coverage )
{
  dbmsg("switch_some_to_summarizing(summ, coverage=%d)", coverage);
  assert( region_group_count( region_group_summzing ) == 0);
#if 0
  /* It may be the "right thing" to wait until enough regions
   * are actually filled to start a summarization cycle
   * so that the payoff is significant.  However, currently
   * turning this code on causes an assertion to fail
   * in sm_construction_progress.  (The assertion attempts to 
   * ensures that setup_next_wave(..) either sets up work 
   * or there is *no* work to possible set up, which does not
   * cover the case that this predicate is catching.
   */
  if (region_group_count( region_group_filled ) < coverage )
    return;
#endif
  while (coverage > 0) {
    old_heap_t *oh = region_group_first_heap( region_group_filled );
    if (oh == NULL) {
      oh = region_group_first_heap( region_group_popular );
      if (oh == NULL) {
        annoyingmsg("summ_matrix.c: ran out of regions to summarize:"
                    " coverage: %3d "
                    " nonrrof: %3d unfilled: %3d wait: %3d"
                    " summ: %3d filled: %3d pop: %3d",
                    coverage, 
                    region_group_count( region_group_nonrrof ),
                    region_group_count( region_group_unfilled ),
                    region_group_count( region_group_wait_w_sum ) + 
                    region_group_count( region_group_wait_nosum ),
                    region_group_count( region_group_summzing ),
                    region_group_count( region_group_filled ),
                    region_group_count( region_group_popular ) );

        /* XXX should above be consolemsg rather than annoyingmsg ??? */

        break; /* nothing left to summarize! */
      }
    }
    dbmsg( "switch_some_to_summarizing : %d ", oh_current_space(oh)->gen_no );
    region_group_enq( oh, oh->group, region_group_summzing );
    assert2( col_words( DATA(summ)->cols
                        [ oh_current_space( oh )->gen_no ] ) == 0 );
    coverage--;
  }
}

EXPORT void sm_start_refinement( summ_matrix_t *summ ) 
{

  dbmsg( "sm_start_refinement( summ )" );

  assert( summ->collector->smircy != NULL );
  assert( smircy_in_construction_stage_p( summ->collector->smircy ));

  smircy_enter_refinement_stage( summ->collector->smircy );

  DATA(summ)->refining.finished_remset_pass_since_start = FALSE;
  { int count = 0;
    int i;
    old_heap_t *oh; 

    oh = region_group_first_heap( region_group_summzing );
    while (oh != NULL) {
      i = oh_current_space( oh )->gen_no;
      DATA(summ)->cols[i]->construction_predates_snapshot = TRUE;
      oh = region_group_next_heap( oh );
      count += 1;
    }

    oh = region_group_first_heap( region_group_wait_w_sum );
    while (oh != NULL) {
      i = oh_current_space( oh )->gen_no;
      DATA(summ)->cols[i]->construction_predates_snapshot = TRUE;
      oh = region_group_next_heap( oh );
      count += 1;
    }

    DATA(summ)->refining.summaries_left_to_process = count;

    annoyingmsg("sm_start_refinement left_to_process: %d", count);
  }
  DATA(summ)->refining.active = TRUE;

}

static void check_if_refining_is_now_done( summ_matrix_t *summ ) 
{
  assert( DATA(summ)->refining.active );
  if ( DATA(summ)->refining.finished_remset_pass_since_start &&
       DATA(summ)->refining.summaries_left_to_process == 0) {
    DATA(summ)->refining.active = FALSE;
    smircy_exit_refinement_stage( summ->collector->smircy );
  }
}

static void sm_build_summaries_iteration_complete( summ_matrix_t *summ,
                                                   int region_count ) 
{
  dbmsg("sm_build_summaries_iteration_complete"
             "( summ, region_count=%d );",
             region_count );

  if (DATA(summ)->refining.active) {
    DATA(summ)->refining.finished_remset_pass_since_start = TRUE;
    check_if_refining_is_now_done( summ );
  }

  /* Update construction complete fields for all 
   * elements of region_group_summzing */
  {
    int i;
    old_heap_t *oh; 
    oh = region_group_first_heap( region_group_summzing );
    while (oh != NULL) {
      i = oh_current_space( oh )->gen_no;
      DATA(summ)->cols[i]->construction_complete = TRUE;
      DATA(summ)->cols[i]->construction_inprogress = FALSE;
      dbmsg( "sm_build_iteration_complete( summ, region_count=%d )"
             ": completed construction on %d",
             region_count, i );
      oh = region_group_next_heap( oh );
    }
  }

  /* count how many successful summaries we constructed.  If it 
   * satisfies our budget, then we're complete.  If budget is
   * not yet satisfied (ie due to popular regions), then 
   * expand summarizing set. */
  {
    int count, goal, total_count;

    goal = DATA(summ)->summarizing.goal;
    count = region_group_count( region_group_summzing );

    /* if count passes goal, we have option of clearing
     * cols[i].  While doing so might be regarded as a lost
     * opportunity, note that we may need to do a new scan
     * anyway (for the next set of summaries) and can gather
     * (slightly) more precise information about cols[i] at that
     * time... (on the other hand, it may be easier to get the
     * code working if I do not clear cols[i]...)
     */

    total_count = 
      (region_group_count( region_group_wait_w_sum ) 
       + region_group_count( region_group_summzing ));

    /* see note above about count potentially passing budget */
    if (total_count >= goal ) {
      dbmsg( "total_count:%d (adding count:%d) meets goal:%d.", 
             total_count, count, goal );
      DATA(summ)->summarizing.complete = TRUE;
      region_group_enq_all( region_group_summzing, region_group_wait_w_sum );
      wait_to_setup_next_wave( summ );
    } else {
      /* count did not meet goal; therefore iterate. */
      int start, upto;
      int coverage;

      dbmsg( "count:%d did not meet goal:%d.", count, goal);

      coverage = 
        (int)ceil(((double)region_count) * DATA(summ)->coverage);
      assert( coverage > 0 );

#if 0
      region_group_enq_all( region_group_summzing, region_group_wait_w_sum );
#endif
      switch_some_to_summarizing( summ, coverage );

      DATA(summ)->summarizing.cursor = summ->collector->gno_count;

      DATA(summ)->pass_count += 1;
      DATA(summ)->curr_pass_units_count = 0;
    }
  }
}

static void sm_clear_col( summ_matrix_t *summ, int i )
{
  { 
    /* clear contribution from summarization */
    clear_col_cells( summ, i );
    /* clear contribution from mutator */
    clear_col_mutator_rs( summ, i );

    col_reset_words( DATA(summ)->cols[i] );
  }

  DATA(summ)->cols[i]->construction_complete = FALSE;
  DATA(summ)->cols[i]->construction_predates_snapshot = FALSE;
}

static void sm_build_summaries_partial_n( summ_matrix_t *summ, 
                                         int rgn_next,
                                         int region_count,
                                         int num )
{
  remset_summary_data_t remsum;
  int gno_count;
  int start;
  int finis;
  int nontrivial_scans;

  dbmsg( "sm_build_summaries_partial( summ, rgn_next=%d, region_count=%d );",
              rgn_next, region_count );

 again:
  remsum.summ = summ;
  remsum.count_objects_visited = 0;
  remsum.count_objects_added = 0;
  remsum.words_added = 0;

  gno_count = summ->collector->gno_count;
  /* Construct
   *   { x | x in summarizing range | x has reference into [fst,lim) }
   */
  finis = DATA(summ)->summarizing.cursor;
  start = max( finis-num, 1 );
  assert2( start < finis );

  nontrivial_scans =
    sm_build_summaries_by_scanning( summ, start, finis, &remsum );

  if (start == 1) {
    sm_build_summaries_iteration_complete( summ, region_count );
    DATA(summ)->summarizing.cursor = gno_count; /* or 1? or 0? */
  } else {
    /* set up cursor for next invocation. */
    assert2( finis <= gno_count );
    assert2( start > 1 );
    DATA(summ)->summarizing.cursor = start;
    if (nontrivial_scans < num) {
      dbmsg("sm_build_summaries_partial_n does a loop"
            " num:%d nontrivial_scans:%d", 
            num, nontrivial_scans );

      /* selects between explicit/implicit tail-call */
#if 0
      sm_build_summaries_partial_n( summ, rgn_next, region_count, 
                                    (num - nontrivial_scans) );
#else
      num = (num - nontrivial_scans);
      goto again;
#endif

    }
  }
}

static void sm_build_summaries_partial( summ_matrix_t *summ, 
                                        int rgn_next,
                                        int region_count,
                                        bool about_to_major )
{
  sm_build_summaries_partial_n( summ, rgn_next, region_count,
                                DATA(summ)->summarizing.num );
}
 
static void sm_build_summaries_just_static_area( summ_matrix_t *summ,
                                                 int rgn_next, 
                                                 int region_count ) 
{
  sm_build_summaries_partial_n( summ, rgn_next, region_count, 1 );
}

static void sm_build_remset_summaries( summ_matrix_t *summ,
                                       int region_count,
                                       int rgn_next,
                                       bool about_to_major )
{
  remset_summary_data_t remsum;
  int i;
  int gno_count = summ->collector->gno_count;
  int goal;

  check_rep_1( summ );

  goal = (int)ceil(((double)region_count) * DATA(summ)->goal);

  remsum.summ = summ;
  remsum.count_objects_visited = 0;
  remsum.count_objects_added = 0;
  remsum.words_added = 0;

  sm_build_summaries_setup( summ, 1, region_count, rgn_next, about_to_major, 
                            0 /* dA does not matter when non-incremental */ );
  sm_build_summaries_by_scanning( summ, 1, gno_count, &remsum );

  sm_build_summaries_iteration_complete( summ, region_count );

  check_rep_1( summ );
}

static void console_printgset( char *prefix, gset_t g ) 
{
  if (g.tag == gs_range) {
    consolemsg("%s(rang,%d,%d)", prefix, g.g1, g.g2 );
  } else if (g.tag == gs_twrng) {
    consolemsg("%s(trng,%d,%d,%d,%d)", prefix, g.g1, g.g2, g.g3, g.g4 );
  } else if (g.tag == gs_singleton) { 
    consolemsg("%s(ones,%d)", prefix, g.g1 );
  } else if (g.tag == gs_nil ) {
    consolemsg("%s(nil)", prefix );
   } else { assert(0); }
}

struct verify_summaries_msgc_fcn_data {
  summ_matrix_t *summ;
  locset_t **summaries;
};

static void* verify_summaries_msgc_fcn( word obj, word src, int byte_offset, 
                                        void *my_data )
{
  struct verify_summaries_msgc_fcn_data *data =
    (struct verify_summaries_msgc_fcn_data*)my_data;
  summ_matrix_t *summ = data->summ;
  locset_t **summaries = data->summaries;
  int src_gen, tgt_gen;

  assert2( (byte_offset % sizeof(word)) == 0 );

  if (isptr(src) && isptr(obj) &&
      ((src_gen = gen_of(src)) != (tgt_gen = gen_of(obj))) &&
      ! gc_is_nonmoving( summ->collector, tgt_gen )) {

    assert( src_gen >= 0 );
    if (src_gen > 0) {
      assert( *summ->collector->ssb[src_gen]->bot == *summ->collector->ssb[src_gen]->top );
      assert( *summ->collector->ssb[tgt_gen]->bot == *summ->collector->ssb[tgt_gen]->top );
      if (region_summarized( summ, tgt_gen ) &&
          ! DATA(summ)->cols[tgt_gen]->overly_popular) {
        loc_t loc;
        loc.obj = src;
        loc.offset = byte_offset;
        assert2( byte_offset < 0 || ((byte_offset % sizeof(word)) == 0));
        if ( ((summaries[ tgt_gen ] == NULL ) )) {
          assertmsg("verify_summaries_msgc_fcn  null summ[i=%d]: src: 0x%08x (%d) obj: 0x%08x (%d)",
                     tgt_gen, src, src_gen, obj, tgt_gen );
        } else if ( ! ls_ismember_loc( summaries[ tgt_gen ], loc )) {
          assertmsg("verify_summaries_msgc_fcn notin summ[i=%d]: src: 0x%08x[%d] (%d) obj: 0x%08x (%d)",
                    tgt_gen, loc.obj, loc.offset, src_gen, obj, tgt_gen );
        }
        assert( summaries[ tgt_gen ] != NULL );
        assert( ls_ismember_loc( summaries[ tgt_gen ], loc ));
      }

      /* XXX when introducing incremental sumz, more (and semi-tricky) logic goes here
       * to validate in-progress summarization. */
      if (region_summarizing_goal( summ, tgt_gen ) &&
          ! DATA(summ)->cols[tgt_gen]->overly_popular &&
          ( DATA(summ)->summarizing.complete 
            || ( ! region_summarizing_curr( summ, tgt_gen ))
            || gen_of(src) >= DATA(summ)->summarizing.cursor )) {
        loc_t loc;
        loc.obj = src;
        loc.offset = byte_offset;
        if ( ((summaries[ tgt_gen ] == NULL ) )) {
          assertmsg("verify_summaries_msgc_fcn  null zing[i=%d]: src: 0x%08x (%d) obj: 0x%08x (%d)",
                     tgt_gen, src, src_gen, obj, tgt_gen );
        } else if ( ! ls_ismember_loc( summaries[ tgt_gen ], loc )) {
          assertmsg("verify_summaries_msgc_fcn notin zing[i=%d]: src: 0x%08x[%d] (%d) obj: 0x%08x (%d)",
                    tgt_gen, loc.obj, loc.offset, src_gen, obj, tgt_gen );
        }
        assert( summaries[ tgt_gen ] != NULL );
        assert( ls_ismember_loc( summaries[ tgt_gen ], loc ));
      }

    }
  }

  return data;
}

struct verify_summaries_locset_fcn_data {
  gc_t *gc;
  msgc_context_t *conserv_context;
  msgc_context_t *aggress_context;
  int summary_for_region; 
  summ_col_t *col;
  summ_matrix_t *summ;
};

static bool msvfy_object_marked_p( msgc_context_t *c, word x ) {
  return msgc_object_marked_p( c, x );
}
static void msvfy_set_object_visitor( msgc_context_t *c, 
                                      void* (*visitor)( word obj, 
                                                        word src,
                                                        int offset, 
                                                        void *data ), 
                                      void *data ) {
  msgc_set_object_visitor( c, visitor, data );
}
static void msvfy_mark_objects_from_roots( msgc_context_t *c ) {
  int marked, traced, words_marked;
  msgc_mark_objects_from_roots( c, &marked, &traced, &words_marked );
}
static void msvfy_mark_objects_from_roots_and_remsets( msgc_context_t *c ) {
  int m, t, wm;
  msgc_mark_objects_from_roots_and_remsets( c, &m, &t, &wm );
}

static bool verify_summaries_locset_fcn( loc_t loc, void *the_data )
{
  struct verify_summaries_locset_fcn_data *data;
  data = (struct verify_summaries_locset_fcn_data*)the_data;
  word obj = loc.obj;

  /* filtering out loc's that are not marked in smircy when we find
   * data->col->construction_predates_snapshot */
  if (data->col->construction_predates_snapshot) {
    assert( data->summ->collector->smircy != NULL );
  }
  if (data->col->construction_predates_snapshot 
      && ! smircy_object_marked_p( data->summ->collector->smircy, 
                                   loc.obj )) {

    /* do nothing (and exit early); the out-dated object is not
     * considered part of the summary and will (or at least should) be
     * filtered out. */
#if 0
    consolemsg( "verify summ rems, SKIP 0x%08x (%d)", 
                loc.obj, gen_of(loc.obj) );
#endif
    return TRUE;
  }

  /* Any object in a summary should be reachable from the 
   * union of roots+remsets */

  if ( msvfy_object_marked_p( data->conserv_context, obj ) &&
       ( gen_of(obj) != data->summary_for_region )) {
  } else {
    consolemsg(  "VERIFY SUMM REMS 0x%08x (%d) "
                 "marked {agg: %s, con: %s, rs[%d]: %s, smircy: %s}", 
                 obj, gen_of(obj), 
                 msvfy_object_marked_p( data->aggress_context, obj )?"Y":"N", 
                 msvfy_object_marked_p( data->conserv_context, obj )?"Y":"N",
                 gen_of(obj),
                 urs_isremembered( data->gc->the_remset, obj )?"Y":"N",
                 ((data->gc->smircy == NULL)?"n/a":
                  (smircy_object_marked_p( data->gc->smircy, obj )?"Y  ":"N  "))
                 );
  }

  assert( msvfy_object_marked_p( data->conserv_context, obj ));
  assert( gen_of(obj) != data->summary_for_region );
  return TRUE;
}

struct scan_check_nursery_ls_data {
  summ_matrix_t  *summ;
  msgc_context_t *conserv_context;
  msgc_context_t *aggress_context;
};

static bool scan_check_nursery_ls( loc_t loc, void *my_data )
{
  struct scan_check_nursery_ls_data *data;
  data = (struct scan_check_nursery_ls_data*)my_data;

  assert( msvfy_object_marked_p( data->conserv_context, loc.obj ));
  return TRUE;
}

static void fold_col_into_locset( summ_matrix_t *summ, int i, locset_t *ls ) 
{
  summ_col_t *col;

  assert( ls->live == 0 );

  col = DATA(summ)->cols[i];
  { /* enumerate objects */
    summ_cell_t *sent = col->cell_top;
    summ_cell_t *curr = sent->next_col;
    while (curr != sent) {
      objs_pool_t *objs = curr->objects;
      assert( objs == NULL ); /* XXX workaround */
      curr = curr->next_col;
    }
  }

  { /* enumerate locations */
    summ_cell_t *sent = col->cell_top;
    summ_cell_t *curr = sent->next_col;
    while (curr != sent) {
      locs_pool_t *locs = curr->locations;
      while (locs != NULL) {
        loc_t *p = locs->bot;
        loc_t *top = locs->top;
        while( p < top ) {
          loc_t l;
          l = *p;
          if (! loc_clear_p( l )) {

            /* note that l.obj may already be present in ls,
               but l should not. */
            assert( ! ls_ismember_loc( ls, l ));
            assert( ! ls_ismember( ls, loc_to_slot(l) ));

            ls_add_obj_offset( ls, l.obj, l.offset );
          }
          p++;
        }
        locs = locs->next;
      }
      curr = curr->next_col;
    }
  }

  if (col->sum_mutator != NULL) {
    ls_copy_all_from( ls, col->sum_mutator );
  }
}

static 
void check_col_on_own_vfy_sm_ls( summ_matrix_t *summ,
                                 int col_idx, 
                                 struct verify_summaries_locset_fcn_data *data)
{
  summ_col_t *col;

  col = DATA(summ)->cols[col_idx];
  data->col = col;

  { /* enumerate objects */
    summ_cell_t *sent = col->cell_top;
    summ_cell_t *curr = sent->next_col;
    while (curr != sent) {
      objs_pool_t *objs = curr->objects;
      assert( objs == NULL );
      curr = curr->next_col;
    }
  }

  { /* enumerate locations */
    /* (note that this approach leads to an *over*-approximation of
     *  the true content of the summary)
     */
    /* (I should probably add a representation for set-of-locations
     *  and use that as the basis of the construction instead of the
     *  hack of using a remset for each column.)
     */
    summ_cell_t *sent = col->cell_top;
    summ_cell_t *curr = sent->next_col;
    while (curr != sent) {
      locs_pool_t *locs = curr->locations;
      while (locs != NULL) {
        loc_t *p = locs->bot;
        loc_t *top = locs->top;
        while( p < top ) {
          if (p->obj != 0x0) {
            verify_summaries_locset_fcn( *p, data );
          }
          p++;
        }
        locs = locs->next;
      }
      curr = curr->next_col;
    }
  }

  if (col->sum_mutator != NULL) {
    ls_enumerate_locs( col->sum_mutator, verify_summaries_locset_fcn, data );
  }
}


EXPORT void sm_verify_summaries_via_oracle( summ_matrix_t *summ )
{
  msgc_context_t *conserv_context;
  msgc_context_t *aggress_context;
  int marked, traced, words_marked;
  locset_t **summaries;

  check_rep_1( summ );

  /* set up simpler summary abstract representation */
  {
    int i;
    summaries = 
      must_malloc( DATA(summ)->num_cols * sizeof(remset_t*));
    for ( i = 1; i < DATA(summ)->num_cols; i++ ) {
      summaries[i] = grab_from_locset_pool();
      verifymsg("col[i=%d]:words{sm:%d,wb:%d,gc:%d,sc:%d}", 
                 i, 
                 DATA(summ)->cols[i]->summarize_word_count,
                 DATA(summ)->cols[i]->writebarr_word_count,
                 DATA(summ)->cols[i]->collector_word_count,
                 DATA(summ)->cols[i]->summacopy_word_count );
      fold_col_into_locset( summ, i, summaries[i] );
      verifymsg("summaries[i=%d]->live: %d", i, summaries[i]->live);
    }
  }

  {
    struct verify_summaries_msgc_fcn_data data;
    data.summ = summ;
    data.summaries = summaries;

    conserv_context = msgc_begin( summ->collector );
    msvfy_set_object_visitor( conserv_context, 
                              verify_summaries_msgc_fcn, 
                              &data );
    /* (useful to have a pre-pass over reachable(roots) so that the
       stack trace tells you whether a problem is due solely to a
       reference chain that somehow involves remembered sets.) */
    msvfy_mark_objects_from_roots( conserv_context );
    /* Summaries are based on (conservative) info in remsets; 
       therefore they may have references to "dead" objects 
       that would not be identified as such if we used 
       only msgc_mark_objects_from_roots
    */
    msvfy_mark_objects_from_roots_and_remsets( conserv_context );

    /* a postpass over the summaries to make sure that their contents
       are sane.
    */
    {
      struct verify_summaries_locset_fcn_data data;
      int i;
      msgc_context_t *aggress_context;
      aggress_context = msgc_begin( summ->collector );
      msvfy_mark_objects_from_roots( aggress_context );
      data.conserv_context = conserv_context;
      data.aggress_context = aggress_context;
      data.gc = summ->collector;
      data.summ = summ;

      for ( i = 1; i < DATA(summ)->num_cols; i++ ) {
        data.summary_for_region = i;
        data.col = DATA(summ)->cols[i];
        check_col_on_own_vfy_sm_ls( summ, i, &data );
      }

      for (i = 0; i < summ->collector->gno_count; i++) {
        if (region_summarized( summ, i )){
          verifymsg("sanity check summary[i=%d] live: %d", 
                     i, summaries[i]->live);
          data.summary_for_region = i;
          data.col = DATA(summ)->cols[i];
          ls_enumerate_locs( summaries[i], 
                             verify_summaries_locset_fcn,
                             &data );
        }
      }

      {
        struct scan_check_nursery_ls_data data;
        data.summ = summ;
        data.conserv_context = conserv_context;
        data.aggress_context = aggress_context;
        ls_enumerate_locs( DATA(summ)->nursery_locset,
                           scan_check_nursery_ls,
                           &data );
      }

      msgc_end( aggress_context );
    }
    msgc_end( conserv_context );
  }

  /* clean up */
  {
    int i;
    for (i = 1; i < DATA(summ)->num_cols; i++) {
      ls_clear( summaries[i] );
      return_to_locset_pool( summaries[i] );
    }
    free( summaries );
  }

  check_rep_1( summ );
}

/* XXX
 * I will need to either traverse the array and remsets for this
 * (potentially with a cache), or maintain the info as summarization
 * and mutation progress.*/
EXPORT int sm_summarized_live( summ_matrix_t *summ, int rgn ) 
{
  bool rgn_summarized;
  int rgn_summarized_live;

  check_rep_3( summ );

  rgn_summarized = region_summarized( summ, rgn );
  if (rgn_summarized) { /* XXX */
    rgn_summarized_live = col_words( DATA(summ)->cols[rgn] );
  } else {
    rgn_summarized_live = -col_words( DATA(summ)->cols[rgn] ) - 1;
  }

  check_rep_3( summ );
  return rgn_summarized_live;
}

static void assert_unreachable( summ_matrix_t *sm, summ_cell_t *ucell )
{
  /* Slow (traverses all of sm), so leave empty except when debugging. */
#if FREE_CELL_ASSERTS_UNREACHABLE
  int i;
  summ_row_t *row;
  summ_col_t *col;
  summ_cell_t *sent, *cell;
  for (i = 0; i < DATA(sm)->num_rows; i++) {
    row = DATA(sm)->rows[i];

    assert( row->cell_lft != ucell );
    assert( row->cell_rgt != ucell );

    sent = row->cell_lft;
    cell = sent;
    do {

      assert( cell != ucell );
      assert( cell->next_row != ucell );
      assert( cell->prev_row != ucell );
      assert( cell->next_col != ucell );
      assert( cell->prev_col != ucell );

      cell = cell->next_row;
    } while ( cell != sent );
  }

  for (i = 0; i < DATA(sm)->num_cols; i++) {
    col = DATA(sm)->cols[i];

    assert( col->cell_top != ucell );
    assert( col->cell_bot != ucell );

    sent = col->cell_top;
    cell = sent;
    do {

      assert( cell != ucell );
      assert( cell->next_row != ucell );
      assert( cell->prev_row != ucell );
      assert( cell->next_col != ucell );
      assert( cell->prev_col != ucell );

      cell = cell->next_col;
    } while ( cell != sent );
  }
#endif
}

static void clear_col_cells( summ_matrix_t *summ, int col_idx )
{
  int rgn_next = col_idx;
  summ_col_t *col = DATA(summ)->cols[ rgn_next ];

  DATA(summ)->row_cache.last_cell_valid = FALSE;

  /* clear contribution from summarization */
  {
    summ_cell_t *sent = col->cell_top;
    summ_cell_t *cell;
    summ_cell_t *next;
    cell = sent->next_col;
    sent->next_col = sent; /* (re-complete the */
    sent->prev_col = sent; /*   now trivial    */
    col->cell_bot = sent;  /*    cycle)        */
    while (cell != sent) {
      next = cell->next_col;
      cell->prev_row->next_row = cell->next_row;
      cell->next_row->prev_row = cell->prev_row;
      cell->prev_col->next_col = cell->next_col;
      cell->next_col->prev_col = cell->prev_col;
      if (DATA(summ)->rows[ cell->source_gno ]->cell_rgt == cell) {
        DATA(summ)->rows[ cell->source_gno ]->cell_rgt = cell->prev_row;
      }
      assert_unreachable( summ, cell ); /* XXX expensive */
      free_cell( cell, 
                 DATA(summ)->entries_per_objs_pool_segment,
                 DATA(summ)->entries_per_locs_pool_segment );
      cell = next;
    }
    assert( (sent->next_col == sent) && (sent->prev_col == sent) );
  }

  if (col->construction_predates_snapshot) {
    col->construction_predates_snapshot = FALSE;
    DATA(summ)->refining.summaries_left_to_process -= 1;
    assert( DATA(summ)->refining.summaries_left_to_process >= 0 );
    check_if_refining_is_now_done( summ );
  }
}

static void clear_col_mutator_rs( summ_matrix_t *summ, int col_idx ) 
{
  int rgn_next = col_idx;
  summ_col_t *col = DATA(summ)->cols[ rgn_next ];
  locset_t *ls = col->sum_mutator;
  if (ls != NULL) {
    ls_clear( ls );
    return_to_locset_pool( ls );
    col->sum_mutator = NULL;
  }
}


struct scan_add_word_data {
  summ_matrix_t *summ;
  locset_t      *to;
  int            to_gen;
};

EXPORT void sm_copy_summary_to( summ_matrix_t *summ, int rgn_next, int rgn_to )
{
  if (region_summarized( summ, rgn_to ) 
      || region_summarizing_goal( summ, rgn_to )) {
    assert(0);

    /* rgn_next == 0 implies minor gc; copy nursery summary to rgn_to,
       but hopefully none of that is actually necessary. */
  }
}

static int count_usable_summaries_in( summ_matrix_t *summ, gset_t gset ) 
{
  int i, s, e, count;
  s = gset_min_elem_greater_than( gset, 0 );
  e = gset_max_elem( gset );
  assert( s > 0 );
  count = 0;
  for (i = s; i <= e; i++) {
    if ( gset_memberp( i, gset ) &&
         ! DATA(summ)->cols[i]->overly_popular &&
         DATA(summ)->cols[i]->construction_complete ) {
      count += 1;
    }
  }
  return count;
}

static void wait_to_setup_next_wave( summ_matrix_t *summ )
{
  DATA(summ)->summarizing.waiting = TRUE;
  DATA(summ)->summarizing.goal = 0;
  DATA(summ)->summarizing.complete = TRUE;
  DATA(summ)->summarizing.cursor = summ->collector->gno_count;
  DATA(summ)->summarizing.num = 0;
}

static void advance_to_next_summary_set( summ_matrix_t *summ,
                                         int rgn_next, 
                                         int region_count,
                                         bool about_to_major,
                                         int dA ) 
{
  int start, coverage, budget, goal_budget;

  assert2( DATA(summ)->summarizing.complete );

  dbmsg( "advance_to_next_summary_set"
         "( summ, rgn_next=%d, region_count=%d, about_to_major=%s, dA=%d );",
         rgn_next, region_count, about_to_major?"TRUE":"FALSE", dA );

  /* 1. Free state associated with prev wave. */
  /* XXX the summaries are freed as they are consued; I do not think 
   * anything is necessary here... */
  assert2( region_group_count(region_group_wait_w_sum) == 0 );

  /* 2. Deploy next wave; prev := next */
  budget = region_group_count( region_group_summzing );
  if (about_to_major && budget > 1) {
    /* if we're about to collect, we've already spent our budget for it. */
    budget -= 1;
  }
  assert2( budget > 0 );
  region_group_enq_all( region_group_summzing, region_group_wait_w_sum );

  assert2( region_group_count(region_group_summzing) == 0 );

  goal_budget = calc_goal( summ, region_count );

  if (region_group_count(region_group_wait_w_sum) > goal_budget) {
    wait_to_setup_next_wave( summ );
  } else {
    /* 3. Set up new next wave. */
    setup_next_wave( summ, rgn_next, region_count, about_to_major, dA );
  }
}

static void setup_next_wave( summ_matrix_t *summ, int rgn_next, 
                             int region_count, bool about_to_major,
                             int dA )
{
  int coverage, budget;

  coverage = max(1,(int)floor(((double)region_count) * DATA(summ)->coverage));
  budget = 
    region_group_count( region_group_wait_w_sum ) +
    region_group_count( region_group_wait_nosum );

  dbmsg("setup_next_wave( summ, rgn_next=%d, region_count=%d ): "
        "coverage:%f=>%d budget:%d",
        rgn_next, region_count, 
        DATA(summ)->coverage, coverage, budget );

  assert2( region_group_count( region_group_summzing ) == 0 );
  switch_some_to_summarizing( summ, coverage );

  sm_build_summaries_setup( summ, budget, region_count, 
                            rgn_next, about_to_major, dA );
  /* a small hack to ensure rs_cursor is always past any newly added areas */
  sm_build_summaries_just_static_area( summ, rgn_next, region_count );

  DATA(summ)->cycle_count += 1;
  DATA(summ)->curr_pass_units_count = 0;
}

static void sm_ensure_available( summ_matrix_t *summ, int gno,
                                 int region_count, bool about_to_major, 
                                 int dA )
{
  bool summed_prev, summed_next;

  check_rep_2( summ );

  summed_prev = sm_is_rgn_summarized( summ, gno );
  summed_next = sm_is_rgn_summarized_next( summ, gno );

  dbmsg( "sm_ensure_available( summ, gno=%d, region_count=%d, dA=%d ):"
              " prev: %s, next: %s", gno, region_count, dA, 
              summed_prev?"TRUE":"FALSE", summed_next?"TRUE":"FALSE");

  if ( sm_is_rgn_summarized( summ, gno )) {
    /* no op */
  } else if ( sm_is_rgn_summarized_next( summ, gno )) {
    advance_to_next_summary_set( summ, gno, region_count, about_to_major, dA );
  } else {
    char *(*f)(region_group_t grp);
    int (*g)(region_group_t grp);
    f = region_group_name;
    g = region_group_count;
    consolemsg("region_group_counts");
    consolemsg("%s:%d %s:%d %s:%d %s:%d %s:%d %s:%d %s:%d",
               f( region_group_nonrrof ),    g( region_group_nonrrof ),
               f( region_group_unfilled ),   g( region_group_unfilled ),
               f( region_group_wait_w_sum ), g( region_group_wait_w_sum ),
               f( region_group_wait_nosum ), g( region_group_wait_nosum ),
               f( region_group_summzing ),   g( region_group_summzing ),
               f( region_group_filled ),     g( region_group_filled ),
               f( region_group_popular ),    g( region_group_popular ));

    consolemsg("failure sm_ensure_available"
               "( summ, gno=%d, region_count=%d, about_to_major=%s, dA=%d )"
               " region_group_of(gno):%s"
               , 
               gno, region_count, (about_to_major?"TRUE":"FALSE"), dA,
               region_group_name
               ( region_group_of( gc_heap_for_gno( summ->collector, gno ))) );

    assert( FALSE );
  }

  check_rep_2( summ );
  return;
}

/* XXX
 * Should be straight-forward. 
 * 
 * This clears a column in the matrix; just return its cells (and
 * hashset) to pool.
 */
EXPORT void sm_clear_summary( summ_matrix_t *summ, int rgn_next, int ne_rgn_count )
{
  dbmsg("sm_clear_summary( summ, rgn_next=%d, ne_rgn_count=%d ):%s",
             rgn_next, ne_rgn_count,
             (sm_is_rgn_summarized( summ, rgn_next )?"Sumz":"Unsm"));

  check_rep_1( summ );

  sm_clear_col( summ, rgn_next );

  check_rep_1( summ );
}

struct filter_objects_from_sum_remset_data {
  gc_t *gc;
  int gen; /* collected gen */
  int col_idx; /* col index of mutator remset */
};
static bool filter_objects_from_sum_remset( word *slot, 
                                            void *the_data )
{
  struct filter_objects_from_sum_remset_data *data;
  data = (struct filter_objects_from_sum_remset_data *)the_data;

  /* When collecting region R, we should never encounter an object
   * from R in the summary for R; summaries contain only objects
   * *outside* of R. */
  assert( data->col_idx != gen_of(slot));

  if (gen_of(slot) == data->gen) {
    return FALSE;
  } else {
    return TRUE;
  }
}
/* 
 * XXX
 * 
 * This is one where things get tricky; it needs to traverse a
 * single *row*.  (It is this computation that is motivating the
 * sparse matrix representation rather than a simple array and hashset
 * per region; see also sm_points_across_callback below.)
 */
EXPORT void sm_clear_contribution_to_summaries( summ_matrix_t *summ, int rgn_next ) 
{
  check_rep_1( summ );

  dbmsg("sm_clear_contribution_to_summaries( summ, rgn_next=%d ) ", rgn_next );

  DATA(summ)->row_cache.last_cell_valid = FALSE;

  /* clear contribution of rgn_next to all columns. */
  {
    summ_row_t *row;
    summ_cell_t *sent;
    summ_cell_t *cell, *next;
    row = DATA(summ)->rows[ rgn_next ];
    sent = row->cell_lft;
    cell = sent->next_row;
    sent->next_row = sent; /* (re-complete the */
    sent->prev_row = sent; /*   now trivial    */
    row->cell_rgt = sent;  /*    cycle)        */
    while (cell != sent) {
      dbmsg("sm_clear_contribution_to_summaries( summ, rgn_next=%d ): "
                 "free_cell( cell{src: %d, tgt:%d} )",
                 rgn_next, cell->source_gno, cell->target_gno );
      next = cell->next_row;
      cell->prev_col->next_col = cell->next_col;
      cell->next_col->prev_col = cell->prev_col;
      cell->prev_row->next_row = cell->next_row;
      cell->next_row->prev_row = cell->prev_row;
      if (DATA(summ)->cols[ cell->target_gno ]->cell_bot == cell) {
        DATA(summ)->cols[ cell->target_gno ]->cell_bot = cell->prev_col;

        /* Keep below in sync with col_incr_words_sm */
        DATA(summ)->cols[ cell->target_gno ]->summarize_word_count -= 
          2*pool_count_objects( cell->objects );
        DATA(summ)->cols[ cell->target_gno ]->summarize_word_count -= 
          pool_count_locations( cell->locations );
      }
      assert_unreachable( summ, cell ); /* XXX expensive */
      free_cell( cell, 
                 DATA(summ)->entries_per_objs_pool_segment,
                 DATA(summ)->entries_per_locs_pool_segment );
      cell = next;
    }

    assert( (sent->next_row == sent) && (sent->prev_row == sent) );

    {
      struct filter_objects_from_sum_remset_data data;
      int i;
      data.gc  = summ->collector;
      data.gen = rgn_next;

      /* XXX see note from Tue Jan 6 16:20:31 EST 2009 
       * should traverse sum_mutator's via linked-list rather than
       * array here. 
       */
      for (i = 0; i < DATA(summ)->num_cols; i++) {
        summ_col_t *col;
        col = DATA(summ)->cols[i];
        data.col_idx = i;
        if ( col->sum_mutator != NULL ) {
          if (col->sum_mutator->live != 0) {
            dbmsg("sm_clear_contribution_to_summaries( summ, rgn_next=%d ): "
                       "filter sum_mutator[col=%d] live: %d",
                       rgn_next, i, col->sum_mutator->live );
            /* XXX fixme use ls_enumerate_locs insted */
            ls_enumerate( col->sum_mutator,
                          filter_objects_from_sum_remset,
                          &data );
            dbmsg("sm_clear_contribution_to_summaries( summ, rgn_next=%d ): "
                       "post filter sum_mu[col=%d] live: %d",
                       rgn_next, i, col->sum_mutator->live );
          } else {
            /* XXX fixme use ls_enumerate_locs insted (and also, WTF?)*/
            ls_enumerate( col->sum_mutator,
                          filter_objects_from_sum_remset,
                          &data );
          }
        }
      }
    }
  }

  check_rep_1( summ );
}

static bool rsenum_filter_tgt_gen( word ptr, void *my_data, unsigned *count ) 
{
  int tgt_gen = *(int*)my_data;
  return (gen_of(ptr) != tgt_gen);
}

/* 
 * This should be fine as long as I continue using a remset_t to
 * represent the nursery summary.  (The only change I anticipate to
 * the nursery summary is perhaps choosing a different variant of
 * hashset, such as cuckoo hashing.)
 */
EXPORT void sm_init_summary_from_nursery_alone( summ_matrix_t *summ, 
                                         summary_t *summary ) 
{
  check_rep_1( summ );
#if 0
  assert_no_forwarded_objects( DATA(summ)->nursery_locset );
#endif
  ls_init_summary( DATA(summ)->nursery_locset, -1, summary);
  check_rep_1( summ );
}

static bool coheres_with_snapshot( summ_matrix_t *summ, 
                                   summ_col_t *col, word w )
{
  bool already_filtered_during_construction;
  already_filtered_during_construction = 
    (! col->construction_predates_snapshot);
  return (already_filtered_during_construction ||
          (isptr(w) &&
           ((gen_of(w) == 0) ||
            smircy_object_marked_p( summ->collector->smircy, w ))));
}

static bool loc_alive_in_snapshot( summ_matrix_t *summ,
                                   summ_col_t *col,
                                   loc_t l )
{
  word *slot, val;
  bool alive_in_snapshot;

  slot = loc_to_slot( l );

  /* A vital assertion!  I have been working under the assumption that
   *       if l.obj is live in the snapshot 
   *     then *loc_to_slot(l) is live in the snapshot
   * (and thus inspecting *loc_to_slot(l) suffices in determining if
   *  the location l should be removed during late refinement)
   *
   * but, that claim is not as trivial as I had once thought.
   */
  assert2( ! isptr(*slot) ||
           ! (coheres_with_snapshot( summ, col, l.obj )
              &&
              ! coheres_with_snapshot( summ, col, *slot )));

  val = *slot; 
  alive_in_snapshot = isptr(val) && coheres_with_snapshot( summ, col, val );

  return alive_in_snapshot;
}

static bool filter_mutated_elems( summary_t *s, word w )
{
  summ_matrix_t *summ   = (summ_matrix_t*) s->cursor1;
  summ_col_t    *col    = (summ_col_t*) s->cursor2;
  locset_t      *nur_rs = DATA(summ)->nursery_locset;
  locset_t      *mut_rs = col->sum_mutator;
  assert(0);
}

static bool filter_mutated_elem_locs( summary_t *s, loc_t l )
{
  summ_matrix_t *summ   = (summ_matrix_t*) s->cursor1;
  summ_col_t    *col    = (summ_col_t*) s->cursor2;
  locset_t      *nur_rs = DATA(summ)->nursery_locset;
  locset_t      *mut_rs = col->sum_mutator;
  word          *slot, val;

  bool alive_in_snapshot;

  slot = loc_to_slot( l );
  alive_in_snapshot = loc_alive_in_snapshot( summ, col, l );

  return (! ls_ismember( mut_rs, slot )) && alive_in_snapshot;
}

static bool filter_gen( summary_t *s, word w )
{
  bool ret = (gen_of(w) != s->icursor2);
  return ret;
}

static bool next_chunk_objs( summary_t *s, 
                             word **start, word **lim, bool *all_unseen)
{
  summ_matrix_t *summ   = (summ_matrix_t*) s->cursor1;
  summ_col_t    *col    = (summ_col_t*)    s->cursor2;

  summ_cell_t *cell = (summ_cell_t*) s->cursor3;
  objs_pool_t *pool = (objs_pool_t*) s->cursor4;

  if (cell == col->cell_top) {
    return FALSE;
  } else {
    *all_unseen = FALSE;
    *start = pool->bot;
    *lim   = pool->top;
    if (pool->next != NULL) {
      s->cursor4 = pool->next;
      return TRUE;
    } else {
      s->cursor4 = cell->next_col->objects;
      s->cursor3 = cell->next_col;
      return TRUE;
    }
  }
}

static bool next_chunk_locs( summary_t *s, 
                             loc_t **start, loc_t **lim, bool *all_unseen)
{
  summ_matrix_t *summ   = (summ_matrix_t*) s->cursor1;
  summ_col_t    *col    = (summ_col_t*)    s->cursor2;

  summ_cell_t *cell = (summ_cell_t*) s->cursor3;
  locs_pool_t *pool = (locs_pool_t*) s->cursor4;

  if (cell == col->cell_top) {
    return FALSE;
  } else {
    *all_unseen = FALSE;
    *start = pool->bot;
    *lim   = pool->top;
    if (pool->next != NULL) {
      s->cursor4 = pool->next;
    } else {
      s->cursor4 = cell->next_col->locations;
      s->cursor3 = cell->next_col;
    }
    return TRUE;
  }
}


static bool filter_icursor1( summary_t *this, word w ) {
  return (gen_of(w) != this->icursor1);
}

struct rs_scan_filter_entries {
  summ_matrix_t *summ;
  int next_gen;
  locset_t *sum_mut;
};
static bool lsscan_filter_entries( word *loc, void *my_data )
{
  struct rs_scan_filter_entries *data;
  bool ret;
  data = (struct rs_scan_filter_entries*)my_data;
  ret = ((gen_of(loc) != data->next_gen) 
          && ! ls_ismember( data->sum_mut, loc ));
  return ret;
}

struct mut_filter_snapshot_data {
  summ_matrix_t *summ;
  summ_col_t *col;
};
static bool mut_filter_snapshot( loc_t l, void *my_data )
{
  struct mut_filter_snapshot_data *data
    = (struct mut_filter_snapshot_data*)my_data;
  summ_matrix_t *summ = data->summ;
  summ_col_t *col = data->col;
  word *slot, val;

  return loc_alive_in_snapshot( summ, col, l );
}

/* 
 * XXX
 * 
 * This will traverse the column for next_summ_idx.
 * (The caller is our "COLsumer").
 */
EXPORT void sm_fold_in_nursery_and_init_summary( summ_matrix_t *summ, 
                                          int next_summ_idx, 
                                          summary_t *summary )
{
  check_rep_1( summ );


  dbmsg("sm_fold_in_nursery_and_init_summary"
             "( summ, next_summ_idx: %d, summary )"
             " construction_predates_snapshot: %s", 
             next_summ_idx, 
             (DATA(summ)->cols[ next_summ_idx ]->construction_predates_snapshot
              ? "T" : "F") );

  {
    summ_col_t *col;
    int entries;
    col = DATA(summ)->cols[ next_summ_idx ];

    if (col->sum_mutator == NULL) {
      /* XXX consider delaying this until its actually 
       * needed within rs_scan_add_word_to_rs */
      col->sum_mutator = grab_from_locset_pool();
    }
#if 0
    {
      /* fold nursery into mutator rs if present */
      struct rs_scan_add_word_to_rs_data data;
      data.summ   = summ;
      data.to_gen = next_summ_idx;
      data.rs_to  = col->sum_mutator;
      rs_enumerate( DATA(summ)->nursery_locset, 
                    rs_scan_add_word_to_rs, 
                    &data );
    }
#else
    {
      struct rs_scan_filter_entries data;
      data.summ     = summ;
      data.next_gen = next_summ_idx;
      data.sum_mut  = col->sum_mutator;
      ls_enumerate( DATA(summ)->nursery_locset, 
                    lsscan_filter_entries, 
                    &data );

      ls_init_summary( DATA(summ)->nursery_locset, -1, 
                       &DATA(summ)->nursery_summary );
    }
#endif

    {
      if (col->construction_predates_snapshot) {
        struct mut_filter_snapshot_data data;
        data.summ = summ;
        data.col = col;
        assert( summ->collector->smircy != NULL );
        assert( smircy_in_refinement_stage_p( summ->collector->smircy ));

        ls_enumerate_locs( col->sum_mutator, 
                           mut_filter_snapshot, &data );
      }

      ls_init_summary( col->sum_mutator, -1,
                       &DATA(summ)->mutrems_summary );
    }

    { 
      entries = col_words(col);
      if (col->cell_top->next_col != col->cell_top) {
        dbmsg("sm_fold_in_nursery_and_init_summary"
                   "(summ, next_summ_idx=%d, summary ) "
                   "cell iter", 
                   next_summ_idx );

        if (col->cell_top->next_col->objects != NULL) {
          summary_init_dispose
            ( &DATA(summ)->summcol_summary_objs, entries, 
              next_chunk_objs, NULL, filter_mutated_elems );
          /* 1: summ, 2: col, 3: cell, 4: objs_pool */
          DATA(summ)->summcol_summary_objs.cursor1 = summ;
          DATA(summ)->summcol_summary_objs.cursor2 = col;
          DATA(summ)->summcol_summary_objs.cursor3 = col->cell_top->next_col;
          DATA(summ)->summcol_summary_objs.cursor4 = 
            col->cell_top->next_col->objects;
        }

        if (col->cell_top->next_col->locations != NULL) {
          summary_init_locs_dispose
            ( &DATA(summ)->summcol_summary_locs, entries, 
              next_chunk_locs, NULL, filter_mutated_elem_locs );
          /* 1: summ, 2: col, 3: cell, 4: objs_pool */
          DATA(summ)->summcol_summary_locs.cursor1 = summ;
          DATA(summ)->summcol_summary_locs.cursor2 = col;
          DATA(summ)->summcol_summary_locs.cursor3 = col->cell_top->next_col;
          DATA(summ)->summcol_summary_locs.cursor4 = 
            col->cell_top->next_col->locations;
        }

        assert( (col->cell_top->next_col->objects != NULL) ||
                (col->cell_top->next_col->locations != NULL) );

        summary_compose( &DATA(summ)->nursery_summary, 
                         &DATA(summ)->mutrems_summary,
                         ((col->cell_top->next_col->objects == NULL) ? NULL 
                          : &DATA(summ)->summcol_summary_objs), 
                         ((col->cell_top->next_col->locations == NULL) ? NULL 
                          : &DATA(summ)->summcol_summary_locs), 
                         summary );
      } else {
        summary_compose( &DATA(summ)->nursery_summary,
                         &DATA(summ)->mutrems_summary, 
                         NULL,
                         NULL,
                         summary );
      }
    }
  }

  check_rep_1( summ );
}

EXPORT void sm_nursery_summary_enumerate( summ_matrix_t *summ, 
                                          bool (*scanner)(loc_t loc, void *data),
                                          void *data )
{
  check_rep_1( summ );
  ls_enumerate_locs( DATA(summ)->nursery_locset, scanner, data );
  check_rep_1( summ );
}


EXPORT bool sm_majorgc_permitted( summ_matrix_t *summ, int rgn_next )
{
  check_rep_3( summ );
  return (col_words( DATA(summ)->cols[ rgn_next ])
          <= DATA(summ)->popularity_limit);
}

/* 
 * XXX
 * standard heap expansion operation.
 * Should just do the straight-forward ("dumb") expansion and
 * hope that's good enough.
 */
static void sm_expand_summary_gnos( summ_matrix_t *summ, int fresh_gno ) 
{
  /* check that inserting fresh_gno does not upset
     the existing summarization data
  */
  {
    /* the incremental remset scan has "already passed" fresh_gno */
    assert( DATA(summ)->summarizing.complete || 
            DATA(summ)->summarizing.cursor <= fresh_gno );
  }

  /* even though inserting the fresh gno will not upset the 
     summarization data, we still need to expand the 
     gc fields related to summaries (because right now 
     I keep that structure proportional to the number of 
     regions
  */
  {
    int len = DATA(summ)->summaries_count+1;
    int i;
    DATA(summ)->summaries_count = len;
  }

#if CONSERVATIVE_REGION_COUNT
  /* check that we will complete summarization on schedule. */
  if (! DATA(summ)->summarizing.complete) {
    int fuel, capability, required;
    fuel = region_group_count( region_group_wait_w_sum );
    capability = DATA(summ)->summarizing.num * fuel;
    required = DATA(summ)->summarizing.cursor;

    assert( capability >= required );
  }
#endif
}

static bool region_summarized( summ_matrix_t *summ, int gno ) 
{
  if (gno == 0) return FALSE;
  return (gc_region_group_for_gno( summ->collector, gno )
          == region_group_wait_w_sum);
}
static bool region_summarizing_goal( summ_matrix_t *summ, int gno ) 
{
  if (gno == 0) return FALSE;
  return (gc_region_group_for_gno( summ->collector, gno )
          == region_group_summzing); 
}
static bool region_summarizing_curr( summ_matrix_t *summ, int gno ) 
{
  if (gno == 0) return FALSE;
  return (gc_region_group_for_gno( summ->collector, gno )
          == region_group_summzing);
}

/* 
 * XXX
 * This is the callback from cheney's scanning of a forwarded object.
 * It is perhaps the most subtle operation to implement.
 * 
 * It will probably be called many times, but I hypothesize:
 * (1.) all invocations for a given lhs are adjacent
 * (2.) any involved lhs is "fresh"
 * 
 * If the above two facts are true, then I should be able to enqueue
 * the lhs in the *array* for g_rhs, rather than the remset for g_rhs.
 * That's important.
 *
 * Note: a gno (for g_rhs) may occur in non-adjacent positions in the
 * invocations (e.g. when scanning an object X = { A1; B2; C1; } where
 * A1,C1 in RGN-1 and B2 in RGN-2).
 *
 * However, I hypothesize:
 * (3.) The set of distinct gen_of(lhs) will be small, and all will
 * occur adjacent to one another in the sequence.
 * 
 * That property is imporant for being able to enqueue in the array for
 * g_rhs *efficiently*; it should allow me to use a similar trick here
 * to the one I plan to use for the summarize pROWduction loop: 
 * keep transient table that maps (SZED u SZING) -> [Maybe Cell].
 */
EXPORT void sm_points_across_callback( summ_matrix_t *summ, 
                                       word lhs, int offset, int g_rhs )
{

  /* Below condition is subtle.
   * 1. If we've already summarized g_rhs and its part of our current budget for
   *    future summarizations, then we will not be summarizing it again and 
   *    must update it's summary.
   * 2. If g_rhs is part of the goal summarizing set, but *not* part of the 
   *    current targets of summarization, then case is analogous to (1) above.
   * 3. If g_rhs is part of the curr summarizing set, but the remset 
   *    cursor has already gone past gen_of(lhs), then case is analogous to (1).
   * 4. If g_rhs is part of the curr summarizing set, but the remset
   *    cursor has not yet encountered gen_of(lhs), then we must *NOT*
   *    enter lhs into the summary, because we are not allowed to 
   *    have duplicate entries in the columns of the summ matrix. 
   *
   * (And maybe this still not quite right, since progress invocations can lead to 
   *  a summarizing.curr_genset that is complete... but I think in that situation
   *  the cursor should be past the gno_count...)
   */
  if ( region_summarized( summ, g_rhs )
       || ( region_summarizing_curr( summ, g_rhs )
            && (DATA(summ)->summarizing.complete || 
                gen_of(lhs) >= DATA(summ)->summarizing.cursor ))
       || ( region_summarizing_goal( summ, g_rhs )
            && ! region_summarizing_curr( summ, g_rhs ))
       ) {

    /* XXX recomputing g_lhs wasteful here (plus it might not be valid
     * yet?  When does cheney change the gno for LOS, before or after
     * scan?); so perhaps change callback to pass g_lhs along too. */
    loc_t loc;
    loc = make_loc( lhs, offset );
    add_loc_to_sum_array( summ, g_rhs, loc, gen_of(lhs), incr_ctxt_gc );
  }
}

EXPORT void sm_before_collection( summ_matrix_t *summ )
{
  if (0) print_matrix( "sm_before_collection", summ );
#if 0
  assert_no_forwarded_objects( DATA(summ)->nursery_locset );
#endif
  check_rep_2( summ );
}

EXPORT void sm_after_collection( summ_matrix_t *summ )
{
  if (0) print_matrix( "sm_after_collection", summ );
#if 0
  assert_no_forwarded_objects( DATA(summ)->nursery_locset );
#endif
  check_rep_2( summ );
}

EXPORT int sm_cycle_count( summ_matrix_t *summ ) { return DATA(summ)->cycle_count; }
EXPORT int sm_pass_count( summ_matrix_t *summ ) { return DATA(summ)->pass_count; }
EXPORT int sm_scan_count_curr_pass( summ_matrix_t *summ ) { return DATA(summ)->curr_pass_units_count; }

static void check_rep( summ_matrix_t *summ ) {
  assert( summ != NULL );
}

/* eof */
