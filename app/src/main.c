/*
 * Lecture 1 - Home task: Scheduler Behavior Explorer
 *
 * Three preemptive threads at different priorities + one cooperative thread.
 *
 *   t_high  prio  3  logs "T_HIGH running", sleeps 100 ms
 *   t_med   prio  5  logs "T_MED running",  sleeps 200 ms
 *   t_low   prio  7  logs "T_LOW running",  sleeps 300 ms
 *   t_coop  prio -1  5 iterations of busy work, then k_yield()
 *
 * What to observe:
 *   - T_HIGH runs most often (every 100 ms), T_LOW least often (every 300 ms).
 *     The *frequency* comes from the sleep time, not from the priority.
 *   - Priority decides the ORDER when several threads become Ready at the
 *     same tick (t=0, 600 ms, 1200 ms ...): HIGH first, then MED, then LOW.
 *   - While the cooperative thread busy-waits, NOTHING else runs - not even
 *     T_HIGH. Its log lines appear late, as a burst, after COOP yields/sleeps.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l1_task1, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_HIGH 3
#define PRIO_MED 5
#define PRIO_LOW 7
#define PRIO_COOP (-1) /* negative = cooperative */

#define COOP_ITERATIONS 5
#define COOP_BUSY_US 20000 /* 20 ms of CPU burning per iteration */

/* Per-thread run counters, printed in the summary */
static atomic_t high_runs;
static atomic_t med_runs;
static atomic_t low_runs;

static void t_high_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
        atomic_inc(&high_runs);
        LOG_INF("T_HIGH running  tick=%u", k_uptime_get_32());
        k_msleep(100); /* Running -> Waiting for 100 ms */
    }
}

static void t_med_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
        atomic_inc(&med_runs);
        LOG_INF("T_MED running   tick=%u", k_uptime_get_32());
        k_msleep(200);
    }
}

static void t_low_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
        atomic_inc(&low_runs);
        LOG_INF("T_LOW running   tick=%u", k_uptime_get_32());
        k_msleep(300);
    }
}

static void t_coop_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* Start a little later so the preemptive threads are already cycling */
    k_msleep(1000);

    while (1)
    {
        uint32_t start = k_uptime_get_32();

        LOG_INF("[COOP] start busy work - nobody can preempt me now");

        for (int i = 0; i < COOP_ITERATIONS; i++)
        {
            /*
             * k_busy_wait() burns CPU without leaving the Running
             * state. A cooperative thread is never preempted by
             * other threads, so T_HIGH misses its 100 ms wake-up.
             */
            k_busy_wait(COOP_BUSY_US);
            LOG_INF("[COOP] iteration %d/%d  tick=%u",
                    i + 1, COOP_ITERATIONS, k_uptime_get_32());
        }

        LOG_INF("[COOP] held CPU for %u ms, now k_yield()",
                k_uptime_get_32() - start);

        /*
         * k_yield(): stay READY, but let other ready threads of
         * equal-or-higher priority run. All our preemptive threads
         * have LOWER priority (3,5,7 > -1), so on its own k_yield()
         * would return straight back to us. That is the lesson:
         * k_yield() != k_sleep().
         */
        k_yield();
        LOG_INF("[COOP] back from k_yield() immediately - still highest ready thread");

        /* Only a real wait (Waiting state) lets lower priorities run */
        k_msleep(2000);
    }
}

K_THREAD_DEFINE(t_high, STACK_SIZE, t_high_fn, NULL, NULL, NULL, PRIO_HIGH, 0, 0);
K_THREAD_DEFINE(t_med, STACK_SIZE, t_med_fn, NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(t_low, STACK_SIZE, t_low_fn, NULL, NULL, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(t_coop, STACK_SIZE, t_coop_fn, NULL, NULL, NULL, PRIO_COOP, 0, 0);

int main(void)
{
    LOG_INF("=== L1 Task: Scheduler Behavior Explorer ===");
    LOG_INF("HIGH=%d/100ms  MED=%d/200ms  LOW=%d/300ms  COOP=%d",
            PRIO_HIGH, PRIO_MED, PRIO_LOW, PRIO_COOP);

    /* Print a summary every 3 s: HIGH ~3x LOW, MED ~1.5x LOW */
    while (1)
    {
        k_msleep(3000);
        LOG_INF("[SUMMARY] runs: HIGH=%ld MED=%ld LOW=%ld",
                atomic_get(&high_runs), atomic_get(&med_runs),
                atomic_get(&low_runs));
    }

    return 0;
}
