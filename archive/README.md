# Archive

Libraries stored in this directory are not used in the project. They are here
just for reference. They were scrapped from various Android images and are not
compatible with Linux.

## aarch64 (aptX)

```shell
patchelf --replace-needed libc.so libc.so.6 aarch64/libaptX-1.0.16-rel-Android21-arm64.so
patchelf --remove-needed libdl.so aarch64/libaptX-1.0.1-rel-Android21-arm64.so
```

## aarch64 (aptX HD)

```shell
patchelf --replace-needed libc.so libc.so.6 aarch64/libaptXHD-1.0.1-rel-Android21-arm64.so
patchelf --remove-needed libdl.so aarch64/libaptXHD-1.0.1-rel-Android21-arm64.so
```
