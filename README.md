# Operating Systems Projects (C & Python)

Systems programming projects I built for CSE 3320 (Operating Systems) at UT Arlington, Spring 2026.

| Project | Language | What it does |
|---|---|---|
| [`lab1-sorting-performance`](lab1-sorting-performance) | C | Times bubble sort vs. quicksort on 35,000 random integers to study speedup and Amdahl's Law |
| [`lab2-file-system`](lab2-file-system) | C | A simulated file system on a virtual disk image (256-byte blocks): `Createfs`, `Formatfs`, `Openfs`, `Savefs`, `List`, `Put`, `Get`, `Remove`, `Rename`, `Link`, `Unlink`, `User` |
| [`lab3-myshell`](lab3-myshell) | C | An interactive Unix shell utility: lists files and directories with paging and sorting (name/size/date), edits files, runs commands, and changes directories by path or number |
| [`lab4-secure-file-storage`](lab4-secure-file-storage) | Python | A Dropbox-style client/server file store over TCP sockets with threaded multi-client support and Fernet (AES) encryption |

## Build & run (C projects)

```bash
gcc -Wall -o myshell lab3-myshell/myshell.c && ./myshell
gcc -Wall -o fsfs lab2-file-system/fsfs.c && ./fsfs
gcc -Wall -O2 -o sortperf lab1-sorting-performance/sorting_perf.c && ./sortperf
```

## Run (Python project)

```bash
pip install cryptography
python lab4-secure-file-storage/server.py   # terminal 1
python lab4-secure-file-storage/client.py   # terminal 2
```

> Built as UT Arlington coursework. If you're taking the course, please don't copy it for your own submission.
