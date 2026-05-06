#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 128
#define MAX_HISTORY 100

typedef struct Command {
    char *argv[MAX_ARGS];
    char *input_file;
    char *output_file;
    bool append_output;
} Command;

static char *history[MAX_HISTORY];
static int history_count = 0;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) {
        ++s;
    }
    if (*s == '\0') {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }
    return s;
}

static void add_history(const char *line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }
    if (history_count == MAX_HISTORY) {
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY; ++i) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    history[history_count] = strdup(line);
    if (history[history_count] != NULL) {
        history_count++;
    }
}

static void print_history(void) {
    for (int i = 0; i < history_count; ++i) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

static void free_history(void) {
    for (int i = 0; i < history_count; ++i) {
        free(history[i]);
    }
}

static void print_help(void) {
    puts("Built-in commands:");
    puts("  cd <dir>      Change directory");
    puts("  pwd           Print current directory");
    puts("  history       Show command history");
    puts("  help          Show help");
    puts("  exit          Exit shell");
    puts("");
    puts("Supported syntax:");
    puts("  command arg1 arg2");
    puts("  command < input.txt");
    puts("  command > output.txt");
    puts("  command >> output.txt");
    puts("  command1 | command2");
}

static void init_command(Command *cmd) {
    for (int i = 0; i < MAX_ARGS; ++i) {
        cmd->argv[i] = NULL;
    }
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = false;
}

static int parse_command(char *line, Command *cmd) {
    init_command(cmd);
    int argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token == NULL) {
                fprintf(stderr, "mini-shell: missing input file\n");
                return -1;
            }
            cmd->input_file = token;
        } else if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0) {
            bool append = strcmp(token, ">>") == 0;
            token = strtok(NULL, " \t\r\n");
            if (token == NULL) {
                fprintf(stderr, "mini-shell: missing output file\n");
                return -1;
            }
            cmd->output_file = token;
            cmd->append_output = append;
        } else {
            if (argc >= MAX_ARGS - 1) {
                fprintf(stderr, "mini-shell: too many arguments\n");
                return -1;
            }
            cmd->argv[argc++] = token;
        }
        token = strtok(NULL, " \t\r\n");
    }

    cmd->argv[argc] = NULL;
    return argc;
}

static int apply_redirection(const Command *cmd) {
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror("open input");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 input");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append_output ? O_APPEND : O_TRUNC;
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror("open output");
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

static int run_builtin(Command *cmd) {
    if (cmd->argv[0] == NULL) {
        return 1;
    }

    if (strcmp(cmd->argv[0], "exit") == 0) {
        free_history();
        exit(0);
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        const char *path = cmd->argv[1];
        if (path == NULL) {
            path = getenv("HOME");
        }
        if (path == NULL) {
            fprintf(stderr, "mini-shell: HOME is not set\n");
            return 1;
        }
        if (chdir(path) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(cmd->argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("pwd");
        } else {
            puts(cwd);
        }
        return 1;
    }

    if (strcmp(cmd->argv[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(cmd->argv[0], "help") == 0) {
        print_help();
        return 1;
    }

    return 0;
}

static int execute_command(Command *cmd) {
    if (cmd->argv[0] == NULL) {
        return 0;
    }

    if (run_builtin(cmd)) {
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        if (apply_redirection(cmd) != 0) {
            _exit(1);
        }
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    return status;
}

static int execute_pipeline(Command *left, Command *right) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork left");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (left_pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            perror("dup2 pipe write");
            _exit(1);
        }
        close(pipefd[1]);
        if (apply_redirection(left) != 0) {
            _exit(1);
        }
        execvp(left->argv[0], left->argv);
        perror("execvp left");
        _exit(127);
    }

    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork right");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (right_pid == 0) {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            perror("dup2 pipe read");
            _exit(1);
        }
        close(pipefd[0]);
        if (apply_redirection(right) != 0) {
            _exit(1);
        }
        execvp(right->argv[0], right->argv);
        perror("execvp right");
        _exit(127);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(left_pid, NULL, 0);
    waitpid(right_pid, NULL, 0);
    return 0;
}

static int run_line(char *line) {
    char *pipe_symbol = strchr(line, '|');

    if (pipe_symbol != NULL) {
        *pipe_symbol = '\0';
        char *left_text = trim(line);
        char *right_text = trim(pipe_symbol + 1);

        Command left;
        Command right;

        if (parse_command(left_text, &left) <= 0) {
            return -1;
        }
        if (parse_command(right_text, &right) <= 0) {
            return -1;
        }
        return execute_pipeline(&left, &right);
    }

    Command cmd;
    if (parse_command(line, &cmd) <= 0) {
        return 0;
    }
    return execute_command(&cmd);
}

int main(void) {
    char line[MAX_LINE];
    puts("mini-shell: type 'help' for built-in commands.");

    while (true) {
        printf("mini-shell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            putchar('\n');
            break;
        }

        char original[MAX_LINE];
        strncpy(original, line, sizeof(original) - 1);
        original[sizeof(original) - 1] = '\0';

        char *clean_line = trim(line);
        char *clean_original = trim(original);

        if (clean_line[0] == '\0') {
            continue;
        }

        add_history(clean_original);
        run_line(clean_line);
    }

    free_history();
    return 0;
}
