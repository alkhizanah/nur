# The Unicode Built-in Module

## Expectations

I expect that you imported the unicode module like that

```
unicode = import("unicode")
```

## Functions

- utf8_encode

Encode the UTF-8 representation to a string

```
unicode.utf8_encode(63) # "?"
unicode.utf8_encode("jdlf") # null
```


- utf8_decode

Decode the UTF-8 representation to an int

```
unicode.utf8_decode("?") # 63
unicode.utf8_decode(2) # null
```

