# Process and Shell Notes

## What is a shell?

A shell is a command-line program that reads user input, parses commands, creates processes, and runs programs.

## Execution Model

```text
read command
parse command
fork child process
exec target program in child process
parent waits for child process
```

In this project:

- `fork()` creates a child process.
- `execvp()` replaces the child process with another program.
- `waitpid()` waits for the child process to finish.

## Redirection

For example:

```bash
echo hello > out.txt
```

The shell uses `open()` and `dup2()` to redirect standard output.

## Pipeline

For example:

```bash
ls | grep .c
```

The shell uses `pipe()` to connect the output of one process to the input of another process.
