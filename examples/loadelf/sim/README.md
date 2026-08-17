# NuttX SIM Loadable ELF Runtime Config

This is an out-of-tree NuttX SIM board configuration for the
`examples/loadelf` runtime and samples.

From the NuttX tree:

```sh
make distclean
./tools/configure.sh -l ../nuttx-apps/examples/loadelf/sim
make -j16
make export
```

Mount `../nuttx-apps` at `/system` from NSH before running samples, so ELF
files installed into `../nuttx-apps/bin` are visible as `/system/bin/<name>`.
