# Installing Nur

## Build from source

Since Nur is a C project that uses Makefile approach, you can just do

```
make -j$(nproc)
```

To release it, or to use it outside of development, you may also want to optimize it:

```
make -j$(nproc) CC="cc -O3"
```
