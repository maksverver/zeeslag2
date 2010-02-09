#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define HEIGHT       16         /* field height */
#define WIDTH        16         /* field width */
#define SHIPS        10         /* number of ships */
#define SHIP_PARTS   30         /* sum of ship lengths */

static const int lengths[SHIPS] = { 5, 4, 4, 3, 3, 3, 2, 2, 2, 2  };

struct Coords { int r, c; };

struct SolverState {
    int rows[HEIGHT], cols[WIDTH];      /* current per-row/column ship counts */
    char blocked[HEIGHT][WIDTH];        /* cells blocked from placing ships */
    char required[HEIGHT][WIDTH];       /* cells on which a ship must be placed */
    int required_left;                  /* number of required cells left */
    int hit_counts[HEIGHT][WIDTH];      /* resulting per-cell hit counts */
};

struct ThreadState {
    struct SolverState ss;              /* thread-local solver state */
    int r1, c1, dir;                    /* row, col, dir of first ship */
};

static inline int min(int a, int b)  { return a < b ? a : b; }
static inline int max(int a, int b)  { return a > b ? a : b; }

static void record_hits(int hits[HEIGHT][WIDTH], int r1, int c1, int r2, int c2, int cnt)
{
    for (int r = r1; r < r2; ++r) {
        for (int c = c1; c < c2; ++c) {
            hits[r][c] += cnt;
        }
    }
}

static inline bool check_counts(int *counts, int size, int a, int b)
{
    for (int i = a; i < b; ++i) {
        if ( counts[i] == 1 &&
             (i - 1 <     0 || counts[i - 1] == 0) &&
             (i + 1 >= size || counts[i + 1] == 0) ) return false;
    }
    return true;
}

static int place(struct SolverState *ss, int ship, int last_r, int last_c)
{
    if (ship == SHIPS) {
        return ss->required_left == 0 ? 1 : 0;
    }

    const int length = lengths[ship];
    if (ship > 0 && length != lengths[ship - 1]) {
        last_r = last_c = 0;
    }

    int res = 0;

    /* Place horizontally: */
    for (int r = last_r; r < HEIGHT ; ++r) {
        if (ss->rows[r] < length) continue;
        for (int c1 = r == last_r ? last_c : 0; c1 <= WIDTH - length; ++c1) {
            const int c2 = c1 + length;
            for (int c = c1; c < c2; ++c) {
                if (ss->cols[c] == 0 || ss->blocked[r][c]) goto h_next;
            }
            const int bc1 = max(c1 - 1, 0), bc2 = min(c2 + 1, WIDTH);
            ss->rows[r] -= length;
            if (!( ss->rows[r] == 1 &&
                   (r     == 0      || ss->rows[r - 1] == 0) &&
                   (r + 1 == HEIGHT || ss->rows[r + 1] == 0) )) {
                for (int c = c1; c < c2; ++c) --ss->cols[c];
                if (check_counts(ss->cols, WIDTH, bc1, bc2)) {
                    if (r - 1 >= 0)     for (int c = bc1; c < bc2; ++c) ++ss->blocked[r - 1][c];
                    if (true)           for (int c = bc1; c < bc2; ++c) ++ss->blocked[r    ][c];
                    if (r + 1 < HEIGHT) for (int c = bc1; c < bc2; ++c) ++ss->blocked[r + 1][c];
                    for (int c = c1; c < c2; ++c) if (ss->required[r][c]++) --ss->required_left;

                    const int cnt = place(ss, ship + 1, r, bc2);
                    if (cnt > 0) record_hits(ss->hit_counts, r, c1, r + 1, c2, cnt);
                    res += cnt;

                    for (int c = c1; c < c2; ++c) if (--ss->required[r][c]) ++ss->required_left;
                    if (r - 1 >= 0)     for (int c = bc1; c < bc2; ++c) --ss->blocked[r - 1][c];
                    if (true)           for (int c = bc1; c < bc2; ++c) --ss->blocked[r    ][c];
                    if (r + 1 < HEIGHT) for (int c = bc1; c < bc2; ++c) --ss->blocked[r + 1][c];
                }
                for (int c = c1; c < c2; ++c) ++ss->cols[c];
            }
            ss->rows[r] += length;
        h_next: continue;
        }
    }

    /* Place vertically: */
    for (int r1 = last_r; r1 <= HEIGHT - length; ++r1) {
        const int r2 = r1 + length;
        for (int r = r1; r < r2; ++r) {
            if (ss->rows[r] == 0) goto v_next_1;
        }
        const int br1 = max(r1 - 1,  0), br2 = min(r2 + 1, HEIGHT);
        for (int c = r1 == last_r ? last_c : 0; c < WIDTH; ++c) {
            if (ss->cols[c] < length) continue;
            for (int r = r1; r < r2; ++r) {
                if (ss->blocked[r][c]) goto v_next_2;
            }

            ss->cols[c] -= length;
            if (!( ss->cols[c] == 1 &&
                   (c == 0         || ss->cols[c - 1] == 0) &&
                   (c + 1 == WIDTH || ss->cols[c + 1] == 0)) ) {
                for (int r = r1; r < r2; ++r) --ss->rows[r];
                if (check_counts(ss->rows, HEIGHT, br1, br2)) {
                    if (c - 1 >= 0)    for (int r = br1; r < br2; ++r) ++ss->blocked[r][c - 1];
                    if (true)          for (int r = br1; r < br2; ++r) ++ss->blocked[r][c];
                    if (c + 1 < WIDTH) for (int r = br1; r < br2; ++r) ++ss->blocked[r][c + 1];
                    for (int r = r1; r < r2; ++r) if (ss->required[r][c]++) --ss->required_left;

                    const int cnt = place(ss, ship + 1, r1, min(c + 2, WIDTH));
                    if (cnt > 0) record_hits(ss->hit_counts, r1, c, r2, c + 1, cnt);
                    res += cnt;

                    for (int r = r1; r < r2; ++r) if (--ss->required[r][c]) ++ss->required_left;
                    if (c - 1 >= 0)    for (int r = br1; r < br2; ++r) --ss->blocked[r][c - 1];
                    if (true)          for (int r = br1; r < br2; ++r) --ss->blocked[r][c];
                    if (c + 1 < WIDTH) for (int r = br1; r < br2; ++r) --ss->blocked[r][c + 1];
                }
                for (int r = r1; r < r2; ++r) ++ss->rows[r];
            }
            ss->cols[c] += length;
        v_next_2: continue;
        }
    v_next_1: continue;
    }

    return res;
}
__attribute__((__noreturn__))
static void fatal(const char *message)
{
    printf("%s\n", message);
    exit(EXIT_FAILURE);  /* success because we produced a line of output */
}

static void *place_first_thread(void *arg)
{
    int cnt = 0;
    struct ThreadState *ts = (struct ThreadState *)arg;
    struct SolverState *ss = &ts->ss;
    const int r1 = ts->r1, c1 = ts->c1;
    const int height = 1 + (lengths[0] - 1)*(ts->dir);
    const int width  = 1 + (lengths[0] - 1)*(1 - ts->dir);
    const int r2 = r1 + height, c2 = c1 + width;
    for (int r = r1; r < r2; ++r) ss->rows[r] -= width;
    for (int c = c1; c < c2; ++c) ss->cols[c] -= height;
    const int br1 = max(r1 - 1, 0), br2 = min(r2 + 1, HEIGHT);
    const int bc1 = max(c1 - 1, 0), bc2 = min(c2 + 1, WIDTH);
    if ( check_counts(ss->rows, HEIGHT, br1, br2) &&
         check_counts(ss->cols, WIDTH, bc1, bc2) ) {
        for (int r = br1; r < br2; ++r) {
            for (int c = bc1; c < bc2; ++c) ++ss->blocked[r][c];
        }
        for (int r = r1; r < r2; ++r) {
            for (int c = c1; c < c2; ++c) {
                if (ss->required[r][c]++) --ss->required_left;
            }
        }
        cnt += place(ss, 1, r1, bc2);
        if (cnt > 0) record_hits(ss->hit_counts, r1, c1, r2, c2, cnt);
        for (int r = r1; r < r2; ++r) {
            for (int c = c1; c < c2; ++c) {
                if (--ss->required[r][c]) ++ss->required_left;
            }
        }
        for (int r = br1; r < br2; ++r) {
            for (int c = bc1; c < bc2; ++c) --ss->blocked[r][c];
        }
    }
    for (int r = r1; r < r2; ++r) ss->rows[r] += width;
    for (int c = c1; c < c2; ++c) ss->cols[c] += height;
    return (void*)(long)cnt;
}

static int place_first(struct SolverState *ss)
{
    struct ThreadState *thread_state[2*HEIGHT*WIDTH];
    pthread_t thread_id[2*HEIGHT*WIDTH];
    int num_threads = 0;

    for (int dir = 0; dir < 2; ++dir) {
        const int height = 1 + (lengths[0] - 1)*dir;
        const int width  = 1 + (lengths[0] - 1)*(1 - dir);

        for (int r1 = 0; r1 <= HEIGHT - height; ++r1) {
            for (int c1 = 0; c1 <= WIDTH - width; ++c1) {
                const int r2 = r1 + height;
                const int c2 = c1 + width;
                bool valid = true;
                for (int r = r1; r < r2; ++r) {
                    if (ss->rows[r] < width) valid = false;
                }
                for (int c = c1; c < c2; ++c) {
                    if (ss->cols[c] < height) valid = false;
                }
                for (int r = r1; r < r2; ++r) {
                    for (int c = c1; c < c2; ++c) {
                        if (ss->blocked[r][c]) valid = false;
                    }
                }

                if (valid) {
                    // Allocate thread state (+1000 to prevent false sharing)
                    struct ThreadState *ts = malloc(sizeof(struct ThreadState) + 1000);
                    memset(ts, 0, sizeof(struct ThreadState));
                    memcpy(&ts->ss, ss, sizeof(struct SolverState));
                    ts->r1 = r1;
                    ts->c1 = c1;
                    ts->dir = dir;
                    thread_state[num_threads] = ts;
                    if (pthread_create(&thread_id[num_threads], NULL, &place_first_thread, ts) != 0)
                        fatal("could not spawn a new thread");
                    num_threads++;
                }
            }
        }
    }

    int total = 0;
    for (int i = 0; i < num_threads; ++i)
    {
        void *res;
        if (pthread_join(thread_id[i], &res) != 0)
            fatal("could not join previously spawned thread");
        total += (int)(long)res;
        for (int r = 0; r < HEIGHT; ++r) {
            for (int c = 0; c < WIDTH; ++c) {
                ss->hit_counts[r][c] += thread_state[i]->ss.hit_counts[r][c];
            }
        }
        free(thread_state[i]);
    }
    return total;
}

static void parse_counts(int *counts, int size, char *data)
{
    int i = 0, total = 0;
    for (char *s = strtok(data, "."); s != NULL; s = strtok(NULL, ".")) {
        if (i >= size) fatal("too many ship counts");
        int val = atoi(s);
        if (val < 0) fatal("negative ship count");
        counts[i++] = val;
        total += val;
    }
    if (i < size) fatal("too few ship counts");
    if (total != SHIP_PARTS) fatal("total ship count is wrong");
}

int main(int argc, char *argv[])
{
    /* Grab command line arguments: */
    bool arg_verbose = false;
    char *arg_rows, *arg_cols, *arg_shots;
    if (argc == 4 && argv[1][0] != '-') {
        arg_verbose = false, arg_rows = argv[1], arg_cols = argv[2], arg_shots = argv[3];
    } else if (argc == 5 && strcmp(argv[1], "-v") == 0) {
        arg_verbose = true, arg_rows = argv[2], arg_cols = argv[3], arg_shots = argv[4];
    } else {
        printf("usage: [-v] %s <Rows> <Cols> <Shots>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Construct initial solver state corresponding with arguments: */
    struct SolverState ss;
    memset(&ss, 0, sizeof(ss));
    parse_counts(ss.rows, HEIGHT, arg_rows);
    parse_counts(ss.cols, WIDTH, arg_cols);
    for (char *s = strtok(arg_shots, "."); s != NULL; s = strtok(NULL, ".")) {
        bool hit = s[0] == 'S';
        if (!hit && s[0] != 'W') continue;
        int col = s[1] - 'A';
        if (col < 0 || col >= 16) continue;
        int row = atoi(&s[2]) - 1;
        if (row < 0 || row >= 16) continue;
        if (ss.required[row][col] || ss.blocked[row][col]) fatal("duplicate shots");
        if (hit) {
            if (ss.required_left == SHIP_PARTS) fatal("too many hits");
            ++ss.required[row][col];
            ++ss.required_left;
        } else {
            ++ss.blocked[row][col];
        }
    }

    /* Solve! */
    int solutions = place_first(&ss);
    if (solutions == 0) fatal("no solutions found");

    /* Figure out firing options using a greedy heuristic: */
    struct Coords options[HEIGHT*WIDTH];
    int num_options = 0, max_hits = 0;
    for (int r = 0; r < HEIGHT; ++r) {
        for (int c = 0; c < WIDTH; ++c) {
            if (ss.required[r][c]) continue;
            if (ss.hit_counts[r][c] > max_hits) {
                max_hits = ss.hit_counts[r][c];
                num_options = 0;
            }
            if (ss.hit_counts[r][c] == max_hits) {
                options[num_options].r = r;
                options[num_options].c = c;
                ++num_options;
            }
        }
    }
    if (num_options == 0) fatal("no cells to fire at");

    if (arg_verbose) {
        /* Print solution count and all possible options: */
        printf("%d %d %d", solutions, max_hits, num_options);
        for (int n = 0; n < num_options; ++n) {
            printf(" %c%d", 'A' + options[n].c, options[n].r + 1);
        }
        printf("\n");
    } else {
        /* Print a random option: */
        srand((unsigned)time(NULL) ^ ((unsigned)getpid() << 16));
        struct Coords coords = options[rand()%num_options];
        printf("%c%d\n", 'A' + coords.c, coords.r + 1);
    }
    return EXIT_SUCCESS;
}
