# The Global Built-in Module

## Functions

- print

Print values to the stdout

Usage:

```
print("Hello, World!")
```

Stdout:

```
Hello, World!
```

- println

Print values then new line to the stdout

Usage:

```
println("Hello, World!")
```

Stdout:

```
Hello, World!

```

- random

Gives you a random float between two values

Usage:

```
random(5, 10) # 7.268858764248719
random(10, 5) # same as above
random(0.1, 0.2) # 0.1783036218123609
random(0, 0) # 0.0
random("", "") # error: expected first argument to be a number, got a string value
```

- exit

Exits from the process with the status code provided

Usage:

```
exit(10) # exits with 10
exit(-2) # exits with 254 instead
exit("") # exits with 1 because "" is provided
```

- typeof

Gives you the type of provided value

Usage:

```
typeof(null) # "null"
typeof("") # "string"
typeof(1) # "int"
typeof(1.5) # "float"
typeof(true) # "bool"
typeof([]) # "array"
typeof({}) # "map"
typeof(print) # "function"
```

- to_int

Casts (string, float, boolean) to int, returns null on failure instead of erroring out

```
to_int(5) # 5
to_int(5.5) # 5
to_int(true) # 1
to_int("5") # 5
to_int("hey") # null
```

- to_float

Casts (int, boolean) to float, returns null on failure instead of erroring out

```
to_float(5.5) # 5.5
to_float(5) # 5.0
to_float(true) # 1.0
to_float("5.5") # 5.5
to_float("hey") # null
```

- to_string

Casts any writeable value to string

```
to_string(5) # "5"
to_string(5.5) # "5.5"
to_string(true) # "true"
to_string("hey") # "hey"
to_string(null) # "null"
to_string([1, 2, 3]) # "[1, 2, 3]"
to_string({"2": 4}) # "{\"2\": 4}"
```

- array_push

Pushes the value provided into an array

```
arr = []

array_push(arr, 5)
array_push(2, 5) # error: expected first argument to be an array to push into, however got an integer value
```

- array_pop

Pops the last value in the array

```
arr = [5]

array_pop(arr) # 5
array_pop(arr) # null
array_pop(2) # error: expected first argument to be an array to pop from, however got an integer value
```

- foreach

Runs the provided callback on (array, string, map) entries

```
arr = [1, 2, 3]

str = "aaa"

map = {1: 2, 3: 4, 4: 5}

foreach(arr, fn v {
    println(v)
})

foreach(str, fn s {
    println(s)
})

foreach(map, fn k, v {
    println(k, v)
})
```

- range

Gives you a ranged array of ints, you can specify the start point, the end point, and the step incrementor

```
# if given a single argument it will use the start point of 0 and the step incrementor of 1
range(4) # [0, 1, 2, 3]

# if given two arguments it will use the first as the starting point and the second as the ending point
range(1, 6) # [1, 2, 3, 4, 5]

# if given three arguments it will act the pass two arugments but it will use the third arugment as the step incrementor
range(0, 12, 2) # [0, 2, 4, 6, 8, 10]
```

- reverse

Gives you a reversed version of the passed iterable (array, string)

```
arr = ["foo", "bar", "foobar", "human"]

str = "hello, world!"

reverse(arr) # ["human", "foobar", "bar", "foo"]

reverse(str) # !dlrow ,olleh
```

- filter

Filter an iterable (array, string, map) based on its member values using a callback, the callback **must** return a boolean value

```
arr = [1, 2, 3, 4, 5, 6]

str = "42not4002"

map = {"cat": 2, "dog": 4, "cow": 8, "car": 34}

filter(arr, fn v { return ((v % 2) == 0) }) # [2, 4, 6]

filter(str, fn s { return s != to_string(0) }) # 42not42

# filter the map based on its keys
filter(map, fn k, v {
    if k != "car" {
        return true
    }

    return false
}) # {"cat": 2, "dog": 4, "cow": 8}

# filter the map based on its values
filter(map, fn k, v {
    if v > 12 {
        return true
    }

    return false
}) # {"car": 34}
```

- transform

Transform (modify) an iterable (array, string, map) based on its member values using a callback, the member will be modified to be equal to the returned value by the callback

```
arr = [1, 2, 3, 4, 5, 6]

str = "42not4002"

map = {"cat": 2, "dog": 4, "cow": 8, "car": 34}

transform(arr, fn v { return v + 1 }) # [2, 3, 4, 5, 6, 7]

transform(str, fn s { return s + "0" }) # 4020n0o0t040000020

# transform the map based on its keys
transform(map, fn k, v {
    if k == "car" {
        return ["toyota", v]
    }

    return [k, v]
}) # {"cat": 2, "dog": 4, "cow": 8, "toyota": 34}

# transform the map based on its values
transform(map, fn k, v {
    if v == 34 {
        return [k, "SUPRA"]
    }

    return [k, v]
}) # {"cat": 2, "dog": 4, "cow": 8, "car": "SUPRA"}
```

- len

Gives you the length of an iterable (array, string, map)

```
arr = [1, 2, 3]

str = "aaa"

map = {1: 2, 3: 4, 4: 5}

len(str) # 3
len(arr) # 3
len(map) # 3
len(89) # null
```

- contains

Checks if a value is in (array values, string sequences, map keys)

```
arr = ["a", "b", "c"]

map = {"a": "b", "c": "d"}

str = "abc"

contains(arr, "ab") # false
contains(arr, "c") # true

contains(map, "ab") # false
contains(map, "c") # true

contains(str, "cd") # false
contains(str, "ab") # true

contains(4, "ab") # null
```

- string_split

Splits the string into multiple strings by the sequence you provided, if the sequence is empty it will split each character instead

```
string_split("Hello world", " ") # ["Hello", "world"]
string_split("world", "") # ["w", "o", "r", "l", "d"]
```

- string_upper

Gives you a uppercased copy of the passed in string

```
string_upper("hello, world") # HELLO, WORLD
```

- string_lower

Gives you a lowercased copy of the passed in string, it acts as the opposite of `string_upper`

```
string_lower("FOO, BAR") # foo, bar
```

- call

Makes you able to call a function programmatically and gives you back its return value by passing its arguments as an array

```
greeter = fn name {
    println("hello, " + name + "!")
}

call(greeter, ["humans"]) # hello, humans!
```

- function_arity

Gives you the number of arguments a function requires, does not work on native functions

```
function_arity(fn foo, bar {}) # 2
function_arity(print) # null
```

- import

Evaluates a file and gives you the exported value (the value that the module `return`ed)

```
import("some_file.nur") # {"hey": 5}
```

- eval

Evaluates a string and gives you the exported value

```
eval("return {\"hey\": 5}") # {"hey": 5}
```

- hash

Gives you the number representation of the hashable value

```
hash(5) # 5
hash("string") # 5389953989438782544
hash([1, 2]) # null
```

- map_keys

Gives you an array of the keys stored in the provided map

```
map = {"1": 2, "3": 4}

println(map_keys(map)) # ["1", "3"]
```

- map_from_keys

Gives you the map with keys you provided and all values are null

```
map = map_from_keys(["1", "3"])

println(map) # {"1": null, "3": null}
```

- map_values

Gives you an array of the values stored in the provided map

```
map = {"": 2, "3": 4}

println(map_values(map)) # [2, 4]
```
