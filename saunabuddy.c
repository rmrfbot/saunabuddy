/*
 * Sauna Buddy -- voting engine prototype (CLI reference implementation)
 *
 * Mirrors the averaging/cooldown rules in index.html so the same
 * algorithm can be checked in plain C before it moves onto whatever
 * hardware ends up driving the actual heater.
 *
 * TEMPERATURE RANGE -- keep in sync with index.html, and configure per
 * facility before deployment. 150-190F sits inside the range public/
 * commercial saunas commonly run (North American Sauna Society cites
 * roughly 158-194F; most sources put a "safe ceiling for healthy
 * adults" around 195F, with heat-illness risk climbing past that).
 * Don't push MAX_TEMP toward water's boiling point (212F) -- no
 * mainstream sauna-safety source treats that as a health target, and
 * a commercial unit serves people with a much wider range of health
 * conditions than a single enthusiast's home sauna does.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_VOTES        200
#define MAX_ID_LEN        32
#define MIN_TEMP          150
#define MAX_TEMP          190
#define VOTE_WINDOW_SECS  (2 * 3600)   /* a vote counts toward the average for 2 hours   */
#define COOLDOWN_SECS     (30 * 60)    /* one new vote per voter every 30 minutes        */

typedef struct {
    char    voter_id[MAX_ID_LEN];
    int     temperature;
    time_t  vote_time;
} Vote;

static Vote votes[MAX_VOTES];
static int  vote_count = 0;

/* Index of a voter's most recent vote, or -1 if they haven't voted. */
static int find_last_vote(const char *voter_id) {
    int last = -1;
    for (int i = 0; i < vote_count; i++) {
        if (strcmp(votes[i].voter_id, voter_id) == 0) {
            if (last == -1 || votes[i].vote_time > votes[last].vote_time) {
                last = i;
            }
        }
    }
    return last;
}

/*
 * Casts a vote as of `when` (pass time(NULL) for a real vote; the
 * "simulate" menu option passes an offset time so the cooldown and
 * window logic can be tested without waiting in real time).
 */
static int cast_vote(const char *voter_id, int temp, time_t when) {
    if (temp < MIN_TEMP || temp > MAX_TEMP) {
        printf("Invalid temperature. Enter a value between %d and %d.\n", MIN_TEMP, MAX_TEMP);
        return 0;
    }

    int last = find_last_vote(voter_id);
    if (last != -1) {
        double since = difftime(when, votes[last].vote_time);
        if (since < COOLDOWN_SECS) {
            printf("Already voted. Try again in %.0f minute(s).\n", (COOLDOWN_SECS - since) / 60.0);
            return 0;
        }
    }

    if (vote_count >= MAX_VOTES) {
        printf("Vote buffer full -- this fixed array is a prototype limit; a real\n"
               "deployment stores votes in a database instead.\n");
        return 0;
    }

    strncpy(votes[vote_count].voter_id, voter_id, MAX_ID_LEN - 1);
    votes[vote_count].voter_id[MAX_ID_LEN - 1] = '\0';
    votes[vote_count].temperature = temp;
    votes[vote_count].vote_time = when;
    vote_count++;
    return 1;
}

/*
 * Same rule as index.html's calculateTarget(): average the votes cast
 * within the last VOTE_WINDOW_SECS; if there are none, fall back to
 * each voter's most recent vote of all time. Returns -1 if there is no
 * data at all. *out_voters receives how many distinct voters were
 * counted, *used_fallback flags which branch was used.
 */
static double target_temperature(time_t now, int *out_voters, int *used_fallback) {
    int recent_sum = 0, recent_n = 0;
    for (int i = 0; i < vote_count; i++) {
        if (difftime(now, votes[i].vote_time) <= VOTE_WINDOW_SECS) {
            recent_sum += votes[i].temperature;
            recent_n++;
        }
    }
    if (recent_n > 0) {
        *out_voters = recent_n;
        *used_fallback = 0;
        return recent_sum / (double)recent_n;
    }

    if (vote_count == 0) {
        *out_voters = 0;
        *used_fallback = 0;
        return -1;
    }

    /* Fallback: latest vote per unique voter, all time. A small-N scan
       is fine for this prototype; a real backend would just query it. */
    static char seen[MAX_VOTES][MAX_ID_LEN];
    static int  latest_temp[MAX_VOTES];
    int seen_count = 0;
    for (int i = 0; i < vote_count; i++) {
        int idx = -1;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(seen[j], votes[i].voter_id) == 0) { idx = j; break; }
        }
        if (idx == -1) {
            strncpy(seen[seen_count], votes[i].voter_id, MAX_ID_LEN - 1);
            seen[seen_count][MAX_ID_LEN - 1] = '\0';
            idx = seen_count++;
        }
        latest_temp[idx] = votes[i].temperature; /* votes are inserted chronologically */
    }

    int sum = 0;
    for (int i = 0; i < seen_count; i++) sum += latest_temp[i];
    *out_voters = seen_count;
    *used_fallback = 1;
    return sum / (double)seen_count;
}

static void print_status(void) {
    int voters = 0, fallback = 0;
    double target = target_temperature(time(NULL), &voters, &fallback);
    if (target < 0) {
        printf("No votes yet.\n");
        return;
    }
    printf("Target temperature: %.1fF  (%d voter%s, %s)\n",
           target, voters, voters == 1 ? "" : "s",
           fallback ? "no recent votes - showing last pick per voter" : "last 2 hours");
}

/* Drains stdin up to and including the next newline, so a bad scanf
   doesn't leave the parser stuck re-reading the same bad token. */
static void flush_input_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

int main(void) {
    char voter_id[MAX_ID_LEN];
    int  choice = 0;

    printf("Sauna Buddy voting prototype (%d-%dF, %d-min cooldown, %d-hour window)\n",
           MIN_TEMP, MAX_TEMP, COOLDOWN_SECS / 60, VOTE_WINDOW_SECS / 3600);

    for (;;) {
        printf("\n1) Cast a vote\n2) Simulate a vote N minutes ago (testing)\n"
               "3) Show target temperature\n4) List all votes\n5) Quit\n> ");
        if (scanf("%d", &choice) != 1) { flush_input_line(); continue; }
        flush_input_line();

        if (choice == 1 || choice == 2) {
            printf("Voter ID (e.g. a name or device id): ");
            if (!fgets(voter_id, sizeof(voter_id), stdin)) continue;
            voter_id[strcspn(voter_id, "\n")] = '\0';
            if (voter_id[0] == '\0') { printf("Voter ID can't be empty.\n"); continue; }

            int temp;
            printf("Temperature (%d-%d F): ", MIN_TEMP, MAX_TEMP);
            if (scanf("%d", &temp) != 1) { printf("Not a number.\n"); flush_input_line(); continue; }
            flush_input_line();

            time_t when = time(NULL);
            if (choice == 2) {
                int minutes_ago;
                printf("Minutes ago: ");
                if (scanf("%d", &minutes_ago) != 1) { printf("Not a number.\n"); flush_input_line(); continue; }
                flush_input_line();
                when = time(NULL) - (time_t)(minutes_ago * 60);
            }

            if (cast_vote(voter_id, temp, when)) {
                printf("Recorded %dF for '%s'.\n", temp, voter_id);
            }
        } else if (choice == 3) {
            print_status();
        } else if (choice == 4) {
            if (vote_count == 0) printf("No votes recorded.\n");
            for (int i = 0; i < vote_count; i++) {
                printf("  %-16s %3dF  (%.0f min ago)\n", votes[i].voter_id, votes[i].temperature,
                       difftime(time(NULL), votes[i].vote_time) / 60.0);
            }
        } else if (choice == 5) {
            break;
        } else {
            printf("Not a menu option.\n");
        }
    }

    return 0;
}
