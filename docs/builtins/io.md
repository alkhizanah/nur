# The Input Output Built-in Module

```
io = import("io")
```

## Constants

- stdin

The Standard Input file descriptor

```
io.stdin # On POSIX-Compatible it is always 0, On Windows it is not stable and depends on the process
```

- stdout

The Standard Output file descriptor

```
io.stdout # On POSIX-Compatible it is always 1, On Windows it is not stable and depends on the process
```

- stderr

The Standard Error file descriptor

```
io.stderr # On POSIX-Compatible it is always 2, On Windows it is not stable and depends on the process
```
