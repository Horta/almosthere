#define ATHR_API_EXPORTS

#include "athr.h"
#include "athr_time.h"
#include "terminal/terminal.h"
#include "thread/thread.h"
#include "widget/bar.h"
#include "widget/eta.h"
#include "widget/line.h"
#include "widget/perc.h"
#include "widget/text.h"
#include <stdio.h>
#include <stdlib.h>

/* Minimum difference in seconds considered safe for computing speed. */
static const double ATHR_MIN_DLT = 0.2;

/* how often to update, in seconds */
static const double ATHR_TIMESTEP = 1.0 / 30.0;

struct athr
{
    long volume;                  /* total size to be consumed */
    long consumed;                /* how much have been consumed */
    double speed;                 /* current consumption speed */
    struct timespec *last_update; /* last timestamp of update call */
    struct timespec delta_start;  /* last timestamp used as delta start */
    long consumed_start;          /* how much was already consumed at delta_start */

    thrd_t thr;      /* spawed thread to perfom update and draw */
    int stop_thread; /* send stop thread signal */

    struct widget *line; /* root widget */
    enum ATHR_OPTS opts; /* display options */
};

void athr_update_speed(struct athr *at);
int athr_thread_start(void *args);
int athr_create_line(struct widget **line, const char *, enum ATHR_OPTS opts);
void athr_update(struct athr *at);

struct athr *athr_create_base(long volume, const char *desc, enum ATHR_OPTS opts)
{

    struct athr *at = NULL;
    int status;

    if (desc == NULL)
        desc = "";

    at = malloc(sizeof(struct athr));

    at->volume = volume;
    at->consumed = 0;
    at->speed = 0;
    at->last_update = NULL;
    athr_timespec_get(&at->delta_start);
    at->consumed_start = 0;
    at->opts = opts;

    if (athr_create_line(&at->line, desc, opts))
        goto err;

    at->stop_thread = 0;
    status = thrd_create(&at->thr, athr_thread_start, at);
    if (status != thrd_success) {
        fprintf(stderr, "Could not spawn a thread.\n");
        goto err;
    }

    return at;
err:
    if (at != NULL)
        free(at);
    return NULL;
}

ATHR_API struct athr *athr_create_var(athr_create_args in)
{
    long volume_out = in.volume ? in.volume : 0;
    const char *desc_out = in.desc ? in.desc : NULL;
    enum ATHR_OPTS opts_out = in.opts ? in.opts : ATHR_BAR | ATHR_ETA | ATHR_PERC;

    return athr_create_base(volume_out, desc_out, opts_out);
}

ATHR_API void athr_consume(struct athr *at, long consume)
{
    if (at->consumed + consume > at->volume) {
        fprintf(stderr, "Trying to consume more than the total volume.\n");
        at->consumed = at->volume;
    } else
        at->consumed += consume;
}

ATHR_API void athr_finish(struct athr *at)
{

    at->stop_thread = 1;
    thrd_join(at->thr, NULL);
    if (at->last_update != NULL)
        free(at->last_update);

    at->line->finish(at->line);

    if (at->volume > at->consumed) {
        printf("\nThe total volume has not been fully consumed.");
    }

    free(at);
    fprintf(stderr, "\n");
}

int athr_thread_start(void *args)
{

    struct athr *at = (struct athr *)args;

    athr_update(at);
    while (at->stop_thread == 0) {
        athr_thread_sleep(ATHR_TIMESTEP);
        athr_update(at);
    }
    athr_update(at);

    return 0;
}

int athr_create_line(struct widget **line, const char *desc, enum ATHR_OPTS opts)
{

    size_t i = 0;
    struct widget *widget[4];

    // widget = malloc(4 * sizeof(struct widget *));
    //
    widget[i++] = widget_text_create(desc);
    if (opts & ATHR_PERC) {
        widget[i++] = widget_perc_create();
    }

    if (opts & ATHR_BAR) {
        widget[i++] = widget_bar_create();
    }

    if (opts & ATHR_ETA) {
        widget[i++] = widget_eta_create();
    }

    *line = widget_line_create(i, widget);

    if (*line == NULL) {
        return 1;
    }

    return 0;
}

void athr_update_speed(struct athr *at)
{
    struct timespec curr, diff;
    long consumed;
    double rate;
    double dlt;

    athr_timespec_get(&curr);
    consumed = at->consumed;
    athr_timespec_diff(&at->delta_start, &curr, &diff);

    dlt = athr_timespec_sec(&diff);

    if (dlt >= ATHR_MIN_DLT) {
        if (consumed > at->consumed_start) {
            rate = (consumed - at->consumed_start) / ((double)at->volume);
            at->speed = rate / dlt;
        } else {
            at->speed /= 2;
        }
        at->delta_start = curr;
        at->consumed_start = consumed;
    }
}

void athr_update(struct athr *at)
{

    struct timespec curr, diff;
    double dlt, frc_consumed;

    athr_update_speed(at);

    athr_timespec_get(&curr);

    if (at->last_update == NULL) {
        at->last_update = malloc(sizeof(struct timespec));
        *at->last_update = curr;
    } // This is the first update call.

    athr_timespec_diff(at->last_update, &curr, &diff);
    dlt = athr_timespec_sec(&diff);

    frc_consumed = ((double)at->consumed) / at->volume;

    at->line->update(at->line, frc_consumed, at->speed, dlt);

    *at->last_update = curr;
}
