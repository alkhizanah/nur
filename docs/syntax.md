# Nur's Syntax

## Calling

- No Arguments
```
callable()
```

- With Arguments
```
callable(1, 2)
```

## Conditional

```
if condition {

} else if another_condition {

} else {

}
```

## While Loop

```
while condition {

}
```

## Comments

```
# This is a comment
```

## Subscript

```
arr = [0, 1, 2, 3, 4, 5]

arr[0] # 0
arr[-1] # 5
arr[6] # error: index out of bounds
arr[-7] # error: index out of bounds

str = "aaa"

str[0] # a

map = {"a": "b"}

map["a"] # "b"
```

## Assignment

- Name Assignment

```
name = "yhya"
```

- Subscript Assignment (Only for Arrays and Maps, Strings are immutable)

```
arr = [0, 1, 2, 3, 4, 5]

arr[0] = 6

map = {"a": "b"}

map["a"] = "c"
```

- Multiple Assignments

```
arr = [0, 1, 2, 3, 4, 5]

map = {"a": "b"}

name = arr[0] = map["name"] = "yhya"
```
