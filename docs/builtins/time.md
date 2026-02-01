# The Time Built-in Module

```
time = import("time")
```

## Functions

- now

Gives you the current time since the unix epoch in seconds

```
time.now() # 1719424371
```

- now_ms

Gives you the current time since the unix epoch in milliseconds

```
time.now_ms() # 1719424371952
```

- sleep

Stops executing for a specific amount of seconds measured in floating points, maximum accuracy is nanoseconds

```
println("Hey")
time.sleep(1)
println("Hey after 1 second")
```
