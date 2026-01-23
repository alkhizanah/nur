# Installing Nur

## Build from source

Since Nur is a C project that uses the unity build approach, and it also uses a kind of rebuilding mechanism when source files change, the building process is very
simple but specific

- Development Build

To build the project for development purposes (like implementing a new feature or fixing some errors), it's only one command:

```
cc -o nur src/one.c -lm
```

When you change any file inside src directory, the program detects this and rebuilds itself, however be careful, the program assumes the current working directory is the same directory that README.md is in, so it will fail to rebuild itself if you changed your current working directory into something else

- Release Build

If you want to use the program outside of this directory or perhaps release it for others to use, you should disable the rebuilding capabilities:

```
cc -o nur src/one.c -lm -DNO_REBUILD
```

You may also want to optimize it:

```
cc -o nur src/one.c -lm -DNO_REBUILD -O3
```
