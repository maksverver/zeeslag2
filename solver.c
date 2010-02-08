#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define SHIPS 10
static const int lengths[SHIPS] = { 5, 4, 4, 3, 3, 3, 2, 2, 2, 2 };

#define MAX_HITS 30
static int g_hits[MAX_HITS];
static int g_num_hits = 0;

struct SolverState {
    int rows[16], cols[16];
    char blocked[16][16];
    int hits[16][16];
};

struct ThreadState {
    struct SolverState ss;
    int r1, c1, dir;
};

static inline int min(int a, int b)  { return a < b ? a : b; }
static inline int max(int a, int b)  { return a > b ? a : b; }

static void count_hits(int hits[16][16], int r1, int c1, int r2, int c2, int cnt)
{
    for (int r = r1; r < r2; ++r) {
        for (int c = c1; c < c2; ++c) {
            hits[r][c] += cnt;
        }
    }
}

static inline bool check_counts(int counts[16], int a, int b)
{
    for (int i = a; i < b; ++i) {
        if (counts[i] == 1 && (i == 0 || counts[i - 1] == 0) && (i + 1 == 16 || counts[i + 1] == 0)) return false;
    }
    return true;
}

int place(struct SolverState *ss, int ship, int last_r, int last_c)
{
    if (ship == SHIPS) {
        return 1;
    }

    const int length = lengths[ship];
    if (ship > 0 && length != lengths[ship - 1]) {
        last_r = last_c = 0;
    }

    int res = 0;

    /* Place horizontally: */
    for (int r = last_r; r < 16 ; ++r) {
        if (ss->rows[r] < length) continue;
        for (int c1 = r == last_r ? last_c : 0; c1 <= 16 - length; ++c1) {
            const int c2 = c1 + length;
            for (int c = c1; c < c2; ++c) {
                if (ss->cols[c] == 0 || ss->blocked[r][c]) goto h_next;
            }
            const int bc1 = max(c1 - 1,  0), bc2 = min(c2 + 1, 16);
            ss->rows[r] -= length;
            if (!(ss->rows[r] == 1 && (r == 0 || ss->rows[r - 1] == 0) && (r + 1 == 16 || ss->rows[r + 1] == 0)))
            {
                for (int c = c1; c < c2; ++c) --ss->cols[c];
                if (check_counts(ss->cols, bc1, bc2))
                {
                    if (r - 1 >= 0) for (int c = bc1; c < bc2; ++c) ++ss->blocked[r - 1][c];
                    if (true)       for (int c = bc1; c < bc2; ++c) ++ss->blocked[r    ][c];
                    if (r + 1 < 16) for (int c = bc1; c < bc2; ++c) ++ss->blocked[r + 1][c];

                    const int cnt = place(ss, ship + 1, r, bc2);
                    if (cnt > 0) count_hits(ss->hits, r, c1, r + 1, c2, cnt);
                    res += cnt;

                    if (r - 1 >= 0) for (int c = bc1; c < bc2; ++c) --ss->blocked[r - 1][c];
                    if (true)       for (int c = bc1; c < bc2; ++c) --ss->blocked[r    ][c];
                    if (r + 1 < 16) for (int c = bc1; c < bc2; ++c) --ss->blocked[r + 1][c];
                }
                for (int c = c1; c < c2; ++c) ++ss->cols[c];
            }
            ss->rows[r] += length;
        h_next: continue;
        }
    }

    /* Place vertically: */
    for (int r1 = last_r; r1 <= 16 - length; ++r1) {
        const int r2 = r1 + length;
        for (int r = r1; r < r2; ++r) {
            if (ss->rows[r] == 0) goto v_next_1;
        }
        const int br1 = max(r1 - 1,  0), br2 = min(r2 + 1, 16);
        for (int c = r1 == last_r ? last_c : 0; c < 16; ++c) {
            if (ss->cols[c] < length) continue;
            for (int r = r1; r < r2; ++r) {
                if (ss->blocked[r][c]) goto v_next_2;
            }

            ss->cols[c] -= length;
            if (!(ss->cols[c] == 1 && (c == 0 || ss->cols[c - 1] == 0) && (c + 1 == 16 || ss->cols[c + 1] == 0)))
            {
                for (int r = r1; r < r2; ++r) --ss->rows[r];
                if (check_counts(ss->rows, br1, br2))
                {
                    if (c - 1 >= 0) for (int r = br1; r < br2; ++r) ++ss->blocked[r][c - 1];
                    if (true)       for (int r = br1; r < br2; ++r) ++ss->blocked[r][c    ];
                    if (c + 1 < 16) for (int r = br1; r < br2; ++r) ++ss->blocked[r][c + 1];

                    const int cnt = place(ss, ship + 1, r1, min(c + 2, 16));
                    if (cnt > 0) count_hits(ss->hits, r1, c, r2, c + 1, cnt);
                    res += cnt;

                    if (c - 1 >= 0) for (int r = br1; r < br2; ++r) --ss->blocked[r][c - 1];
                    if (true)       for (int r = br1; r < br2; ++r) --ss->blocked[r][c    ];
                    if (c + 1 < 16) for (int r = br1; r < br2; ++r) --ss->blocked[r][c + 1];
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

void *place_first_thread(void *arg)
{
    struct ThreadState *ts = (struct ThreadState *)arg;
    struct SolverState *ss = &ts->ss;
    const int r1 = ts->r1, c1 = ts->c1;
    const int height = 1 + (lengths[0] - 1)*(ts->dir);
    const int width  = 1 + (lengths[0] - 1)*(1 - ts->dir);
    const int r2 = r1 + height, c2 = c1 + width;
    for (int r = r1; r < r2; ++r) ss->rows[r] -= width;
    for (int c = c1; c < c2; ++c) ss->cols[c] -= height;
    const int br1 = max(r1 - 1,  0), bc1 = max(c1 - 1,  0);
    const int br2 = min(r2 + 1, 16), bc2 = min(c2 + 1, 16);
    if (!check_counts(ss->rows, br1, br2)) return 0;
    if (!check_counts(ss->cols, bc1, bc2)) return 0;
    for (int r = br1; r < br2; ++r) {
        for (int c = bc1; c < bc2; ++c) ++ss->blocked[r][c];
    }
    const int cnt = place(ss, 1, r1, bc2);
    if (cnt > 0) count_hits(ss->hits, r1, c1, r2, c2, cnt);
    for (int r = br1; r < br2; ++r) {
        for (int c = bc1; c < bc2; ++c) --ss->blocked[r][c];
    }
    for (int r = r1; r < r2; ++r) ss->rows[r] += width;
    for (int c = c1; c < c2; ++c) ss->cols[c] += height;
    return (void*)(long)cnt;
}

int place_first(struct SolverState *ss)
{
    const int max_threads = 2*16*16;
    struct ThreadState *thread_state[max_threads];
    pthread_t thread_id[max_threads];
    int num_threads = 0;

    for (int dir = 0; dir < 2; ++dir) {
        const int height = 1 + (lengths[0] - 1)*dir;
        const int width  = 1 + (lengths[0] - 1)*(1 - dir);

        for (int r1 = 0; r1 <= 16 - height; ++r1) {
            for (int c1 = 0; c1 <= 16 - width; ++c1) {
                const int r2 = r1 + height;
                const int c2 = c1 + width;
                for (int r = r1; r < r2; ++r) if (ss->rows[r] < width) goto next;
                for (int c = c1; c < c2; ++c) if (ss->cols[c] < height) goto next;
                for (int r = r1; r < r2; ++r) {
                    for (int c = c1; c < c2; ++c) if (ss->blocked[r][c]) goto next;
                }

                // Allocate thread state (+1000 to prevent false sharing)
                struct ThreadState *ts = malloc(sizeof(struct ThreadState) + 1000);
                memset(ts, 0, sizeof(struct ThreadState));
                memcpy(&ts->ss, ss, sizeof(struct SolverState));
                ts->r1 = r1;
                ts->c1 = c1;
                ts->dir = dir;
                thread_state[num_threads] = ts;
                int error = pthread_create(&thread_id[num_threads], NULL, &place_first_thread, ts);
                assert(error == 0);
                num_threads++;
            next: continue;
            }
        }
    }

    printf("%d threads\n", num_threads);

    int total = 0;
    for (int i = 0; i < num_threads; ++i)
    {
        void *res;
        int error = pthread_join(thread_id[i], &res);
        assert(error == 0);
        total += (int)(long)res;
        for (int r = 0; r < 16; ++r) {
            for (int c = 0; c < 16; ++c) {
                ss->hits[r][c] += thread_state[i]->ss.hits[r][c];
            }
        }
        free(thread_state[i]);
    }
    return total;
}

int main()
{
    struct SolverState ss = {
        /* 42 */
        // { 2,1,2,2,2,4,0,1,1,2,1,5,0,1,3,3 },
        // { 3,3,2,1,1,1,5,2,2,1,4,1,1,1,1,1 }

        /* 194073 */
        // { 2,4,0,0,5,0,4,0,0,5,0,0,7,0,0,3 }
        // { 1,2,2,2,2,2,2,3,2,2,2,2,1,2,2,1 },

        /* 6346 */
        // { 3,2,0,3,2,2,0,3,0,0,5,0,0,4,0,6 },
        // { 1,1,1,1,2,3,3,1,2,2,4,3,2,2,1,1 }

        /* 956448 (3.9s) */
        // { 0,2,0,4,0,5,0,0,4,2,0,3,0,5,0,5 },
        // { 1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1 }

        /* 677548 (4.3s) */
        { 2,0,4,0,3,0,5,0,5,0,4,0,2,5,0,0 },
        { 1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1 }

        /* 1,562,616 (5.4s) */
        // { 5,0,5,0,4,0,2,0,2,0,5,0,3,0,4,0 },
        // { 1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1 }
    };

    int cnt = place_first(&ss);
    printf("cnt=%d\n", cnt);
    if (cnt > 0) {

        for (int r = 0; r < 16; ++r) {
            for (int c = 0; c < 16; ++c) {
                printf("%4.2f ", 1.0*ss.hits[r][c]/cnt);
            }
            printf("\n");
        }
    }
    /* TODO: select a maximal square to fire at */
}
