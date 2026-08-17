# Loadable ELF Samples

These samples build loadable NuttX ELF modules outside the NuttX apps build
framework.  They use the full SDK exported by `make export`.

The samples use the compiler and linker settings recorded by `make export`.
Override `CC=...`, `CXX=...`, or `LD=...` on the make command line to use
another tool.

The C++ stress sample uses the libc++/libc++abi runtime provided by the NuttX
firmware.  Build the `examples/loadelf/sim` configuration first so NuttX prepares
its LLVM libc++ sources, builds `libxx.a`, exports the libc++ headers, and
exports the C++ symbols needed by the loadable ELF.  The sample also declares
its own `nx_stacksize` ELF symbol because it needs more stack than the default
loadable ELF stack.

From the NuttX tree:

```sh
make distclean
./tools/configure.sh -l ../nuttx-apps/examples/loadelf/sim
make -j16
make export
loadelf_dir=../nuttx-apps/examples/loadelf
rm -rf "$loadelf_dir/nuttx-export"
export_tarball=$(ls -t nuttx-export-*.tar.gz | head -n1)
tar -xzf "$export_tarball" -C "$loadelf_dir"
mv "$loadelf_dir/${export_tarball%.tar.gz}" \
  "$loadelf_dir/nuttx-export"
```

Build and install all samples, then regenerate `../symbols.txt` from their
undefined symbols:

```sh
make -C ../nuttx-apps/examples/loadelf/samples
```

Run sample checks:

```sh
make -C ../nuttx-apps/examples/loadelf/samples check
```

After `symbols.txt` changes, rebuild the NuttX firmware so the generated
symbol table is linked into the image.

Run from NuttX sim:

```sh
mount -t hostfs -o fs=../nuttx-apps /system
cd /system/bin
./loadelf_sample 10 20 30
./loadelf_cpp_stress
qjs /system/examples/loadelf/samples/loadelf_js_sample.js 10 20 30
```
