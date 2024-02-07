# openpower-hw-isolation

In OpenPOWER based system, a user or application can isolate hardware and the
respective isolated hardware will be ignored to init during the next boot of the
host.

**Note:**

- The system must be in a power-off state when a user wants to isolate or
  deisolate hardware.
- The isolated hardware details will be considered only in the next boot of the
  host to isolate.

### To build

```
meson builddir
ninja -C builddir
```


test a
