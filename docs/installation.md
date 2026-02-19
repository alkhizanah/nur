# Installing Nur

## Build from source

Since Nur is a C project that uses [mkc](https://github.com/alkhizanah/mkc) build system, use:

```
mkc -j $(nproc)
```

The built binary will be located at `build/nur`

And to release it, or to use it outside of development, you may also want to optimize it:

```
mkc -j $(nproc) -O 3
```
