#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define BG_MAX 8
#define AR_MAX 17
#define MAX_CMDS 8
#define FILE_MAX 32

void strist(char *str, char ch, int pos) {
    int len = strlen(str);
    if (pos < 0 || pos > len) {
        printf("Invalid position\n");
        return;
    }

    for (int i = len; i >= pos; i--) {
        str[i + 1] = str[i];
    }

    str[pos] = ch;
}

void pre_parse(char *str) {
    char *I_ptr = strchr(str, '<');
    if (I_ptr) {
        if (I_ptr - str == 0 || (*(I_ptr - 1) != ' ')) {
            strist(str, ' ', I_ptr - str);
            I_ptr += 1;
        }
        if (*(I_ptr + 1) != ' ') {
            strist(str, ' ', I_ptr - str + 1);
        }
    }
    char *O_ptr = strchr(str, '>');
    if (O_ptr) {
        if (O_ptr - str == 0 || (*(O_ptr - 1) != ' ')) {
            strist(str, ' ', O_ptr - str);
            O_ptr += 1;
        }
        if (*(O_ptr + 1) != ' ') {
            strist(str, ' ', O_ptr - str + 1);
        }
    }
    char *ptr = str;
    char *P_ptr = strchr(ptr, '|');
    while (P_ptr) {
        if (*(P_ptr + 1) != ' ') {
            strist(str, ' ', P_ptr - str + 1);
        }
        if (P_ptr - ptr == 0 || (*(P_ptr - 1) != ' ')) {
            strist(str, ' ', P_ptr - str);
            P_ptr += 1;
        }
        ptr = P_ptr + 1;
        P_ptr = strchr(ptr, '|');
    }
}

int main(void) {
    char cmd[CMDLINE_MAX];
    char *eof;
    char cmd_buf[CMDLINE_MAX];
    char bg_jobs[BG_MAX][CMDLINE_MAX];
    pid_t bg_pids[BG_MAX];
    int bg_count = 0;
    while (1) {
        char *nl;
        char *argv[AR_MAX];
        for (int i = 0; i < bg_count;) {
            int status;
            pid_t bg_pid = bg_pids[i];
            pid_t res = waitpid(bg_pid, &status, WNOHANG);
            if (res > 0) {
                fprintf(stderr, "+ completed '%s' [%d]\n", bg_jobs[i], status);
                fflush(stderr);
                bg_pids[i] = bg_pids[bg_count - 1];
                strcpy(bg_jobs[i], bg_jobs[bg_count - 1]);
                bg_count--;
            } else {
                i++;
            }
        }

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        eof = fgets(cmd, CMDLINE_MAX, stdin);
        if (!eof)
            /* Make EOF equate to exit */
            strncpy(cmd, "exit\n", CMDLINE_MAX);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        strcpy(cmd_buf, cmd);
        if (nl)
            *nl = '\0';
        /* Builtin command */
        if (!strcmp(cmd, "exit")) {
            if (bg_count != 0) {
                fprintf(stderr, "Error: active job still running\n");
                fprintf(stderr, "+ completed 'exit' [1]\n");
                fflush(stderr);
                continue;
            } else {
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed 'exit' [0]\n");
                fflush(stderr);
                break;
            }
        }
        int pos = 0;
        char *ptr;
        cmd_buf[strlen(cmd_buf) - 1] = ' ';
        pre_parse(cmd_buf);
        ptr = cmd_buf;
        while (*ptr == ' ') {
            ptr++;
        }

        char *end = strchr(ptr, ' ');
        int cmd_num = 1;
        char file[FILE_MAX];
        int redirect = -1;
        int I_redirect = -1;
        int I_end = -1;
        // int I_flag = 1;
        char *bg_pos = strchr(ptr, '&');
        if (bg_pos) {
            char *temp = cmd_buf + strlen(cmd_buf) - 1;
            while (*temp && *temp == ' ') {
                temp--;
            }
            if (bg_pos != temp) {
                fprintf(stderr, "Error: mislocated background sign\n");
                fflush(stderr);
                continue;
            }
            *bg_pos = ' ';
        }

        while (end) {
            *end = '\0';
            argv[pos] = ptr;
            if (strcmp(argv[pos], "|") == 0) {
                if (I_redirect != -1 && I_end == -1) {
                    I_end = pos;
                }
                cmd_num += 1;
                argv[pos] = NULL;
            } else if (strcmp(argv[pos], ">") == 0) {
                redirect = pos;
                argv[pos] = NULL;
            } else if (strcmp(argv[pos], "<") == 0) {
                if (cmd_num != 1) {
                    // I_flag = 0;
                }
                I_redirect = pos;
                argv[pos] = NULL;
            }
            pos++;
            ptr = end + 1;
            while (*ptr && (*ptr == ' ')) {
                ptr++;
            }
            end = strchr(ptr, ' ');
        }
        if (I_redirect != -1 && I_end == -1) {
            I_end = pos;
        }
        int len = pos;
        argv[len] = NULL;
        int rdfd;
        if (redirect != -1) {
            if (redirect == len - 1) {
                fprintf(stderr, "Error: no output file\n");
                fflush(stderr);
                continue;
            }
            if (redirect != len - 2) {
                fprintf(stderr, "Error: mislocated output redirection\n");
                fflush(stderr);
                continue;
            }
            if (redirect == 0) {
                fprintf(stderr, "Error: missing command\n");
                fflush(stderr);
                continue;
            }
            strcpy(file, argv[len - 1]);
            rdfd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (rdfd < 0) {
                fprintf(stderr, "Error: cannot open output file\n");
                fflush(stderr);
                continue;
            }
        }
        int irdfd;
        if (I_redirect != -1) {
            if (argv[I_redirect + 1] == NULL) {
                fprintf(stderr, "Error: no input file\n");
                fflush(stderr);
                continue;
            }
            if (I_redirect != -1 && (cmd_num > 1 || I_redirect < 1)) {
                fprintf(stderr, "Error: mislocated input redirection\n");
                fflush(stderr);
                continue;
            }
            if (I_redirect == 0) {
                fprintf(stderr, "Error: missing command\n");
                fflush(stderr);
                continue;
            }
            strcpy(file, argv[len - 1]);
            irdfd = open(file, O_RDONLY);
            if (irdfd < 0) {
                fprintf(stderr, "Error: cannot open input file\n");
                fflush(stderr);
                continue;
            }
        }
        if (len > 16) {
            fprintf(stderr, "Error: too many process arguments\n");
            fflush(stderr);
            continue;
        }
        if (argv[0] == NULL) {
            if (cmd_num != 1) {
                fprintf(stderr, "Error: missing command\n");
                fflush(stderr);
            }
            continue;
        }

        // pwd
        if (strcmp(argv[0], "pwd") == 0) {
            char cwd[CMDLINE_MAX];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
            fprintf(stderr, "+ completed 'pwd' [0]\n");
            fflush(stdout);
            fflush(stderr);
            continue;
        }

        // cd
        if (strcmp(argv[0], "cd") == 0) {
            if (chdir(argv[1]) < 0) {
                fprintf(stderr, "Error: cannot cd into directory\n");
                fprintf(stderr, "+ completed '%s' [1]\n", cmd);
                fflush(stderr);
                continue;
            } else {
                fprintf(stderr, "+ completed '%s' [0]\n", cmd_buf);
                fflush(stderr);
            }
            continue;
        }

        // execute
        int pipefds[2 * (MAX_CMDS - 1)];
        for (int i = 0; i < cmd_num - 1; i++) {
            pipe(pipefds + i * 2);
        }
        char **cmd_begin = argv;
        int running = 0;
        pid_t pids[MAX_CMDS];
        for (int i = 0; i < cmd_num; i++) {
            if (cmd_begin[0] == NULL && i != cmd_num - 1) {
                fprintf(stderr, "Error: missing command\n");
                fflush(stderr);
                break;
            }
            pids[i] = fork();
            if (pids[i] == 0) {
                if (i > 0) {
                    dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
                    // close(pipefds[(i - 1) * 2]);
                }
                if (i < cmd_num - 1) {
                    dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
                    // close(pipefds[i * 2 + 1]);
                }
                if (redirect != -1 && i == cmd_num - 1) {
                    dup2(rdfd, STDOUT_FILENO);
                    close(rdfd);
                }
                if (I_redirect != -1 && i == 0) {
                    dup2(irdfd, STDIN_FILENO);
                    close(irdfd);
                }
                for (int i = 0; i < (cmd_num - 1) * 2; i++) {
                    close(pipefds[i]);
                }
                if (execvp(cmd_begin[0], cmd_begin) < 0) {
                    fprintf(stderr,"Error: command not found\n");
                    exit(1);
                };
                exit(0);
            }
            while (*cmd_begin != NULL) {
                cmd_begin++;
            }
            cmd_begin++;
            running++;
        }
        for (int i = 0; i < (cmd_num - 1) * 2; i++) {
            close(pipefds[i]);
        }
        if (redirect != -1) {
            close(rdfd);
        }
        if (I_redirect != -1) {
            close(irdfd);
        }
        if (bg_pos != NULL) {
            strcpy(bg_jobs[bg_count], cmd);
            bg_pids[bg_count] = pids[cmd_num - 1];
            bg_count++;
        } else {
            int statuss[running];
            for (int i = 0; i < running; i++) {
                waitpid(pids[i], statuss + i, 0);
            }
            if (running < cmd_num) {
                continue;
            }
            fprintf(stderr, "+ completed '%s' [", cmd);
            for (int i = 0; i < cmd_num; i++) {
                if (i != cmd_num - 1) {
                    fprintf(stderr, "%d][", statuss[i]);
                } else {
                    fprintf(stderr, "%d]\n", statuss[i]);
                }
            }
        }
        fflush(stdout);
        fflush(stderr);
    }

    return EXIT_SUCCESS;
}
