/*
Name: Diyanshu Goli

Description:
This program implements a simple Unix shell utility that displays
files and directories in the current working directory and allows
operations such as editing files, running commands, changing
directories, sorting, paging, and refreshing the display.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#define MAX_ENTRIES 1024
#define MAX_NAME_LEN 512
#define PAGE_SIZE 5

typedef struct
{
    char name[MAX_NAME_LEN];
    off_t size;
    time_t mtime;
} Entry;

void clear_input_buffer()
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}

void pause_screen()
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    clear_input_buffer();
}

void load_entries(Entry files[], int *file_count, Entry dirs[], int *dir_count)
{
    DIR *d;
    struct dirent *de;
    struct stat st;

    *file_count = 0;
    *dir_count = 0;

    d = opendir(".");
    if (d == NULL)
    {
        perror("opendir");
        return;
    }

    while ((de = readdir(d)) != NULL)
    {
        if (strcmp(de->d_name, ".") == 0)
        {
            continue;
        }

        if (stat(de->d_name, &st) != 0)
        {
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (*dir_count < MAX_ENTRIES)
            {
                strncpy(dirs[*dir_count].name, de->d_name, MAX_NAME_LEN - 1);
                dirs[*dir_count].name[MAX_NAME_LEN - 1] = '\0';
                dirs[*dir_count].size = st.st_size;
                dirs[*dir_count].mtime = st.st_mtime;
                (*dir_count)++;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (*file_count < MAX_ENTRIES)
            {
                strncpy(files[*file_count].name, de->d_name, MAX_NAME_LEN - 1);
                files[*file_count].name[MAX_NAME_LEN - 1] = '\0';
                files[*file_count].size = st.st_size;
                files[*file_count].mtime = st.st_mtime;
                (*file_count)++;
            }
        }
    }

    closedir(d);
}

void sort_by_name(Entry arr[], int n)
{
    int i, j;
    Entry temp;
    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - i - 1; j++)
        {
            if (strcmp(arr[j].name, arr[j + 1].name) > 0)
            {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void sort_by_size(Entry arr[], int n)
{
    int i, j;
    Entry temp;
    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - i - 1; j++)
        {
            if (arr[j].size > arr[j + 1].size)
            {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void sort_by_date(Entry arr[], int n)
{
    int i, j;
    Entry temp;
    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - i - 1; j++)
        {
            if (arr[j].mtime > arr[j + 1].mtime)
            {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void display_entries(Entry arr[], int count, int start_index)
{
    int i;
    int end_index = start_index + PAGE_SIZE;

    if (end_index > count)
    {
        end_index = count;
    }

    for (i = start_index; i < end_index; i++)
    {
        printf("%d. %s\n", i, arr[i].name);
    }

    if (count == 0)
    {
        printf("(none)\n");
    }
}

void show_screen(Entry files[], int file_count, Entry dirs[], int dir_count, int page_start)
{
    char cwd[1024];
    time_t t;

    system("clear");

    t = time(NULL);
    printf("Time: %s", ctime(&t));

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("\nCurrent Directory: %s\n", cwd);
    }
    else
    {
        perror("getcwd");
    }

    printf("----------------------------------\n");

    printf("\nDirectories:\n");
    display_entries(dirs, dir_count, page_start);

    printf("\nFiles:\n");
    display_entries(files, file_count, page_start);

    printf("\n----------------------------------\n");
    printf("Operations:\n");
    printf("E Edit File\n");
    printf("R Run Command\n");
    printf("C Change Directory by Path\n");
    printf("M Move to Directory by Number\n");
    printf("S Sort Listing\n");
    printf("N Next Page\n");
    printf("P Previous Page\n");
    printf("D Refresh Display\n");
    printf("Q Quit\n");
}

int main(int argc, char *argv[])
{
    Entry files[MAX_ENTRIES];
    Entry dirs[MAX_ENTRIES];

    int file_count = 0;
    int dir_count = 0;
    int page_start = 0;

    char choice;
    char input[1024];
    char cmd[1024];
    int num;

    if (argc > 1)
    {
        if (chdir(argv[1]) != 0)
        {
            perror("chdir");
        }
    }

    load_entries(files, &file_count, dirs, &dir_count);
    sort_by_name(files, file_count);
    sort_by_name(dirs, dir_count);

    while (1)
    {
        if (page_start < 0)
        {
            page_start = 0;
        }

        show_screen(files, file_count, dirs, dir_count, page_start);

        printf("\nEnter Command: ");
        if (scanf(" %c", &choice) != 1)
        {
            printf("Input error.\n");
            break;
        }
        clear_input_buffer();

        if (choice == 'Q' || choice == 'q')
        {
            printf("Exiting shell...\n");
            break;
        }
        else if (choice == 'D' || choice == 'd')
        {
            load_entries(files, &file_count, dirs, &dir_count);
            sort_by_name(files, file_count);
            sort_by_name(dirs, dir_count);
            if (page_start >= file_count && page_start >= dir_count)
            {
                page_start = 0;
            }
        }
        else if (choice == 'N' || choice == 'n')
        {
            if (page_start + PAGE_SIZE < file_count || page_start + PAGE_SIZE < dir_count)
            {
                page_start += PAGE_SIZE;
            }
        }
        else if (choice == 'P' || choice == 'p')
        {
            if (page_start - PAGE_SIZE >= 0)
            {
                page_start -= PAGE_SIZE;
            }
            else
            {
                page_start = 0;
            }
        }
        else if (choice == 'E' || choice == 'e')
        {
            printf("Enter file number: ");
            if (scanf("%d", &num) != 1)
            {
                printf("Invalid input.\n");
                clear_input_buffer();
                pause_screen();
                continue;
            }
            clear_input_buffer();

            if (num >= 0 && num < file_count)
            {
                char *editor = getenv("EDITOR");
                if (editor == NULL)
                {
                    editor = "nano";
                }

                snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, files[num].name);
                system(cmd);

                load_entries(files, &file_count, dirs, &dir_count);
                sort_by_name(files, file_count);
                sort_by_name(dirs, dir_count);
            }
            else
            {
                printf("Invalid file number.\n");
                pause_screen();
            }
        }
        else if (choice == 'R' || choice == 'r')
        {
            printf("Enter command to run: ");
            if (fgets(input, sizeof(input), stdin) == NULL)
            {
                printf("Input error.\n");
                pause_screen();
                continue;
            }

            input[strcspn(input, "\n")] = '\0';

            if (strlen(input) == 0)
            {
                printf("No command entered.\n");
                pause_screen();
                continue;
            }

            system(input);
            pause_screen();
        }
        else if (choice == 'C' || choice == 'c')
        {
            printf("Enter directory path: ");
            if (fgets(input, sizeof(input), stdin) == NULL)
            {
                printf("Input error.\n");
                pause_screen();
                continue;
            }

            input[strcspn(input, "\n")] = '\0';

            if (chdir(input) != 0)
            {
                perror("chdir");
                pause_screen();
            }
            else
            {
                page_start = 0;
                load_entries(files, &file_count, dirs, &dir_count);
                sort_by_name(files, file_count);
                sort_by_name(dirs, dir_count);
            }
        }
        else if (choice == 'M' || choice == 'm')
        {
            printf("Enter directory number: ");
            if (scanf("%d", &num) != 1)
            {
                printf("Invalid input.\n");
                clear_input_buffer();
                pause_screen();
                continue;
            }
            clear_input_buffer();

            if (num >= 0 && num < dir_count)
            {
                if (chdir(dirs[num].name) != 0)
                {
                    perror("chdir");
                    pause_screen();
                }
                else
                {
                    page_start = 0;
                    load_entries(files, &file_count, dirs, &dir_count);
                    sort_by_name(files, file_count);
                    sort_by_name(dirs, dir_count);
                }
            }
            else
            {
                printf("Invalid directory number.\n");
                pause_screen();
            }
        }
        else if (choice == 'S' || choice == 's')
        {
            int sort_choice;

            printf("Sort by:\n");
            printf("1. Name\n");
            printf("2. Size\n");
            printf("3. Date\n");
            printf("Enter choice: ");

            if (scanf("%d", &sort_choice) != 1)
            {
                printf("Invalid input.\n");
                clear_input_buffer();
                pause_screen();
                continue;
            }
            clear_input_buffer();

            if (sort_choice == 1)
            {
                sort_by_name(files, file_count);
                sort_by_name(dirs, dir_count);
            }
            else if (sort_choice == 2)
            {
                sort_by_size(files, file_count);
                sort_by_size(dirs, dir_count);
            }
            else if (sort_choice == 3)
            {
                sort_by_date(files, file_count);
                sort_by_date(dirs, dir_count);
            }
            else
            {
                printf("Invalid sort choice.\n");
                pause_screen();
            }
        }
        else
        {
            printf("Invalid command.\n");
            pause_screen();
        }
    }

    return 0;
}