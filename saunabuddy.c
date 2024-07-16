#include <stdio.h>
#include <time.h>

#define MAX_USERS 10
#define MIN_TEMP 160
#define MAX_TEMP 212

typedef struct {
    int temperature;
    time_t vote_time;
} Vote;

Vote votes[MAX_USERS];
int vote_count = 0;

void add_vote(int temp) {
    if (vote_count < MAX_USERS) {
        votes[vote_count].temperature = temp;
        votes[vote_count].vote_time = time(NULL);
        vote_count++;
    } else {
        printf("Maximum number of votes reached.\n");
    }
}

void calculate_percentage() {
    time_t current_time = time(NULL);
    int recent_votes = 0;

    for (int i = 0; i < vote_count; i++) {
        if (difftime(current_time, votes[i].vote_time) <= 3 * 3600) {
            recent_votes++;
        }
    }

    if (recent_votes > 0) {
        printf("Percentage of votes in the last 3 hours: %.2f%%\n", (recent_votes / (float)vote_count) * 100);
    } else {
        printf("No votes in the last 3 hours.\n");
    }
}

void calculate_average_temperature() {
    int total_temp = 0;

    for (int i = 0; i < vote_count; i++) {
        total_temp += votes[i].temperature;
    }

    if (vote_count > 0) {
        printf("Average temperature: %.2f°F\n", total_temp / (float)vote_count);
    } else {
        printf("No votes to calculate average temperature.\n");
    }
}

int main() {
    int temp;
    char choice;

    while (vote_count < MAX_USERS) {
        printf("Enter the temperature you want to vote for (between %d and %d Fahrenheit): ", MIN_TEMP, MAX_TEMP);
        scanf("%d", &temp);

        if (temp >= MIN_TEMP && temp <= MAX_TEMP) {
            add_vote(temp);
        } else {
            printf("Invalid temperature. Please enter a value between %d and %d.\n", MIN_TEMP, MAX_TEMP);
        }

        if (vote_count >= 1) {
            printf("Do you want to add another vote? (y/n): ");
            scanf(" %c", &choice);

            if (choice == 'n' || choice == 'N') {
                break;
            }
        }
    }

    calculate_percentage();
    calculate_average_temperature();

    return 0;
}
