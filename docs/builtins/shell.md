# The Shell Built-in Module

```
shell = import("shell")
```

## Functions

- run

Run a command

```
shell.run("echo Hello, World!") # { "termination": "exited", "status_code": 0, "stdout": "Hello, World!\n", "stderr": "Hello, World!\n" }
```
