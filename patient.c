#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PATIENT_FILE "data/patients.txt"
#define DONOR_FILE   "data/donors.txt"
#define STOCK_FILE   "data/stock.txt"
#define REQUEST_FILE "data/requests.txt"
#define ISSUE_FILE   "data/issues.txt"

#define TEMP_STOCK   "data/stock_temp.txt"
#define TEMP_REQUEST "data/request_temp.txt"

void patient_init(void)
{
    FILE *fp;

    fp = fopen(PATIENT_FILE, "a");
    if (fp) fclose(fp);

    fp = fopen(DONOR_FILE, "a");
    if (fp) fclose(fp);

    fp = fopen(REQUEST_FILE, "a");
    if (fp) fclose(fp);

    fp = fopen(ISSUE_FILE, "a");
    if (fp) fclose(fp);

    fp = fopen(STOCK_FILE, "r");

    if (fp == NULL)
    {
        fp = fopen(STOCK_FILE, "w");

        if (fp)
        {
            fprintf(fp, "A+|0\n");
            fprintf(fp, "A-|0\n");
            fprintf(fp, "B+|0\n");
            fprintf(fp, "B-|0\n");
            fprintf(fp, "AB+|0\n");
            fprintf(fp, "AB-|0\n");
            fprintf(fp, "O+|0\n");
            fprintf(fp, "O-|0\n");
            fclose(fp);
        }
    }
    else
    {
        fclose(fp);
    }
}

int valid_blood_group(const char *blood)
{
    const char *groups[] = {
        "A+", "A-", "B+", "B-",
        "AB+", "AB-", "O+", "O-"
    };

    int i;

    for (i = 0; i < 8; i++)
    {
        if (strcmp(blood, groups[i]) == 0)
            return 1;
    }

    return 0;
}

void patient_add(
    const char *name,
    const char *age,
    const char *blood,
    const char *severity
)
{
    FILE *fp;

    if (!name || !age || !blood || !severity)
        return;

    if (!valid_blood_group(blood))
        return;

    fp = fopen(PATIENT_FILE, "a");

    if (fp == NULL)
        return;

    fprintf(fp, "%s|%s|%s|%s\n",
            name, age, blood, severity);

    fclose(fp);
}

void donor_add(
    const char *name,
    const char *age,
    const char *blood
)
{
    FILE *fp;

    if (!name || !age || !blood)
        return;

    if (!valid_blood_group(blood))
        return;

    fp = fopen(DONOR_FILE, "a");

    if (fp == NULL)
        return;

    fprintf(fp, "%s|%s|%s\n",
            name, age, blood);

    fclose(fp);
}

/* Add units to available stock */
int stock_add(const char *blood, int units)
{
    FILE *fp;
    FILE *temp;
    char line[200];
    char group[20];
    int quantity;
    int found = 0;

    if (!valid_blood_group(blood) || units <= 0)
        return 0;

    fp = fopen(STOCK_FILE, "r");
    temp = fopen(TEMP_STOCK, "w");

    if (fp == NULL || temp == NULL)
    {
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%19[^|]|%d",
                   group, &quantity) == 2)
        {
            if (strcmp(group, blood) == 0)
            {
                quantity += units;
                found = 1;
            }

            fprintf(temp, "%s|%d\n", group, quantity);
        }
    }

    if (!found)
        fprintf(temp, "%s|%d\n", blood, units);

    fclose(fp);
    fclose(temp);

    if (remove(STOCK_FILE) != 0 ||
        rename(TEMP_STOCK, STOCK_FILE) != 0)
    {
        return 0;
    }

    return 1;
}

/* Deduct units only if sufficient stock exists */
int deduct_stock(const char *blood, int units)
{
    FILE *fp;
    FILE *temp;
    char line[200];
    char group[20];
    int quantity;
    int found = 0;
    int enough = 0;

    if (!valid_blood_group(blood) || units <= 0)
        return 0;

    fp = fopen(STOCK_FILE, "r");

    if (fp == NULL)
        return 0;

    temp = fopen(TEMP_STOCK, "w");

    if (temp == NULL)
    {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%19[^|]|%d",
                   group, &quantity) == 2)
        {
            if (strcmp(group, blood) == 0)
            {
                found = 1;

                if (quantity >= units)
                {
                    quantity -= units;
                    enough = 1;
                }
            }

            fprintf(temp, "%s|%d\n", group, quantity);
        }
    }

    fclose(fp);
    fclose(temp);

    if (!found || !enough)
    {
        remove(TEMP_STOCK);
        return 0;
    }

    if (remove(STOCK_FILE) != 0 ||
        rename(TEMP_STOCK, STOCK_FILE) != 0)
    {
        return 0;
    }

    return 1;
}

/* Record a successful blood issue */
int issue_blood(
    const char *patient,
    const char *blood,
    int units
)
{
    FILE *fp;
    time_t now;
    struct tm *timeinfo;
    char date_time[50];

    if (!patient || !blood || units <= 0)
        return 0;

    if (!valid_blood_group(blood))
        return 0;

    fp = fopen(ISSUE_FILE, "a");

    if (fp == NULL)
        return 0;

    if (!deduct_stock(blood, units))
    {
        fclose(fp);
        return 0;
    }

    now = time(NULL);
    timeinfo = localtime(&now);

    if (timeinfo)
        strftime(date_time, sizeof(date_time),
                 "%Y-%m-%d %H:%M:%S", timeinfo);
    else
        strcpy(date_time, "Time unavailable");

    fprintf(fp, "%s|%s|%d|%s\n",
            patient, blood, units, date_time);

    fclose(fp);

    return 1;
}

/* Add a pending blood request */
void request_add(
    const char *patient,
    const char *blood,
    int units
)
{
    FILE *fp;

    if (!patient || !blood || units <= 0)
        return;

    if (!valid_blood_group(blood))
        return;

    fp = fopen(REQUEST_FILE, "a");

    if (fp == NULL)
        return;

    fprintf(fp, "%s|%s|%d|QUEUED\n",
            patient, blood, units);

    fclose(fp);
}

/* Process the oldest pending request */
int request_process(void)
{
    FILE *fp;
    FILE *temp;
    char line[300];
    char patient[120];
    char blood[20];
    char status[30];
    int units;
    int processed = 0;

    fp = fopen(REQUEST_FILE, "r");

    if (fp == NULL)
        return 0;

    temp = fopen(TEMP_REQUEST, "w");

    if (temp == NULL)
    {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line,
                   "%119[^|]|%19[^|]|%d|%29[^\n]",
                   patient, blood, &units, status) == 4)
        {
            if (!processed && strcmp(status, "QUEUED") == 0)
            {
                if (issue_blood(patient, blood, units))
                {
                    fprintf(temp, "%s|%s|%d|ISSUED\n",
                            patient, blood, units);
                    processed = 1;
                }
                else
                {
                    fprintf(temp, "%s|%s|%d|INSUFFICIENT_STOCK\n",
                            patient, blood, units);
                    processed = 2;
                }
            }
            else
            {
                fprintf(temp, "%s", line);
            }
        }
        else
        {
            fprintf(temp, "%s", line);
        }
    }

    fclose(fp);
    fclose(temp);

    if (remove(REQUEST_FILE) != 0 ||
        rename(TEMP_REQUEST, REQUEST_FILE) != 0)
    {
        return 0;
    }

    return processed;
}

/* Read a text file into a Python-readable output buffer */
void read_file(
    const char *filename,
    char *output,
    int size,
    int queued_only,
    int history_only
)
{
    FILE *fp;
    char line[400];

    if (output == NULL || size <= 0)
        return;

    output[0] = '\0';

    fp = fopen(filename, "r");

    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp))
    {
        if (queued_only && strstr(line, "QUEUED") == NULL)
            continue;

        if (history_only && strstr(line, "ISSUED") == NULL)
            continue;

        if ((int)strlen(output) + (int)strlen(line) < size - 1)
            strcat(output, line);
    }

    fclose(fp);
}

void patient_list(char *output, int size)
{
    read_file(PATIENT_FILE, output, size, 0, 0);
}

void donor_list(char *output, int size)
{
    read_file(DONOR_FILE, output, size, 0, 0);
}

void stock_list(char *output, int size)
{
    read_file(STOCK_FILE, output, size, 0, 0);
}

void request_list(char *output, int size)
{
    read_file(REQUEST_FILE, output, size, 1, 0);
}

void request_history(char *output, int size)
{
    read_file(ISSUE_FILE, output, size, 0, 0);
}
