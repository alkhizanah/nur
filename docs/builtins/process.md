# The Process Built-in Module

## Expectations

I expect that you imported the process module like that

```
process = import("process")
```

## Constants

- argv

The arguments provided to `nur run` command as an array of strings

- env

The environment variables map

```
process.env # {"HOME": "/home/yhya"}
```
