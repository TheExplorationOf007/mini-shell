# Mini Shell

A tiny Unix-like shell written in C for learning operating systems, Linux system calls, and process management.

This project is designed to be small, readable, and easy to extend. It is suitable for a computer science student who has learned C, operating systems, and Linux.

## Features

- Execute external commands with `fork`, `execvp`, and `waitpid`
- Built-in commands: `cd`, `pwd`, `exit`, `help`, `history`
- Input redirection with `<`
- Output redirection with `>` and `>>`
- Single pipeline support, such as `ls | grep .c`
- Simple command history display

## Directory Structure

```text
mini-shell/
├── .github/workflows/build.yml
├── docs/process-and-shell.md
├── src/main.c
├── .gitignore
├── Makefile
├── README.md
└── LICENSE
```

## Build

```bash
make
```

## Run

```bash
./mini-shell
```

## Example

```bash
mini-shell> pwd
mini-shell> ls -l
mini-shell> echo hello > out.txt
mini-shell> cat < out.txt
mini-shell> ls | grep README
mini-shell> history
mini-shell> exit
```

## What I Learned

- How a shell creates child processes
- How `fork`, `execvp`, and `waitpid` work together
- How `dup2` redirects standard input and output
- How pipes connect two processes

## Future Improvements

- Support multiple pipelines
- Add signal handling for Ctrl+C
- Add command auto-completion
- Add better quote parsing
