# Installing Nur

## Build from source

Since Nur is a C project that uses the unity build approach, you can just do

```
cc -o nur src/one.c -lm
```

To release it, or to use it outside of development, you may also want to optimize it:

```
cc -o nur src/one.c -lm -O3
```
