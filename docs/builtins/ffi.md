# The Foreign Function Interface Built-in Module

```
ffi = import("ffi")
```

## Constants

- types

Contains a map of types to choose from, available keys are:

[

    void,

    u8,
    u16,
    u32,
    u64,

    i8,
    i16,
    i32,
    i64,

    f32,
    f64,

    pointer,

]

- dll.suffix

Can be "so" or "dll" or "dylib" depending on the platform

## Functions

- dll.open

Opens a dynamic link library, searches in the libraries directory of the user by default (e.g. /usr/lib), use a relative path if you want it to be based on the current working directory (e.g. "./libadd.so")

```
libc = ffi.dll.open("libc.so.6", {
    "exit": {
        "parameters": [ffi.types.i32],
        "returns": ffi.types.void,
    },

    "puts": {
        "parameters": [ffi.types.pointer],
        "returns": ffi.types.void,
    },
})
```

- dll.close

Closes an open dynamic link library

```
ffi.dll.close(libc)
```

- to_cstring

Casts a Nur string to a C string (which is also called a "null-terminated string")

```
libc.puts(ffi.to_cstring("Hello, world!"))
```

- allocate_callback

Allocates a FFI Callback that can be passed to a function that accepts a specific function pointer

Warning: You should call `free_callback` on the output because the garbage collector will not handle this!

```
print_i = fn (i) {
    print(i)
}

print_i_callback = ffi.allocate_callback(print_i, {
    "parameters": [ffi.types.i32],
    "returns": ffi.types.void,
})
```

- free_callback

Frees the FFI Callback (the writeable and executable memory especially)

```
ffi.free_callback(print_i_callback)
```
