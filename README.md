# Lua 5.4 for Ultra Engine

**Current version: 5.4.4**

You can use this version of Lua for building modules for use with Ultra Engine.

The only change is in luaconf.h:
```c
#define LUA_IDSIZE	4096
```

The library is compiled as a static library, with no DLL.

Visual Studio project and precompiled binary are provided.

Lua modules are ready-to-compile in the Modules folder.
