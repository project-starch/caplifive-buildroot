# Persistent application guest

Enable `BR2_PACKAGE_CAPSTONE_RUNTIME` and set
`BR2_PACKAGE_CAPSTONE_RUNTIME_SOURCE` to the absolute LLVM Capstone checkout.
The local CMake package copies `capstone/runtime` into the Buildroot output; it
builds and installs `/usr/bin/capstone-exec` and `/usr/bin/capstone-job`, selects
modcapstone and Dropbear, and installs `S40capstone`.

Use the matching `domain-process-runtime` QEMU/monitor revisions. The top-level
`make TARGET=qemu build` generates the monitor with `CAPSTONE_SUPERVISED_CALL`.
The FPGA target does not enable this extension. Initialize the pinned submodules
and use the existing setup/build procedure, then enable this package through
Buildroot's menuconfig. This lane pins published OpenSBI/monitor forks because
the upstream repositories are read-only to the lane; their URLs are recorded in
`.gitmodules`, so recursive checkout remains sufficient. The source path is machine-local configuration, not a
checked-in absolute path.

The boot service loads `/capstone.ko`, selects/checks the process ABI through
`capstone-exec --stats`, and optionally mounts the `hostshare` 9p export at
`/mnt/host`. The ordinary Linux serial shell can then run any application ABI v1
image. SSH provisioning and QMP lifecycle are provided by the LLVM tree's
`capstone-vm` host package. Installed images need no per-launch copying of the
module, launcher or SSH server.

Validation used a clean Buildroot output and the existing prepared RISC-V Linux
6.1 kernel/toolchain, supplied as an external toolchain and `LINUX_DIR`. It built
modcapstone, both launcher programs, Dropbear and a fresh 128 MiB ext2 rootfs.
That rootfs was booted without component overrides and exercised by the common
application gate. This was not a rebuild of the compiler or Linux kernel.

The module retains a bounded physical pool (`process_cache_bytes`, default
384 MiB). Final file/VMA release revokes/scrubs resources for reuse; cached pages
are deliberately not returned to Linux while the monitor retains their carved
authority. Cached storage pins the module until reboot. `capstone-exec --stats`
reports live ownership separately. The legacy globally addressed API and process
API cannot coexist in one module lifetime. Legacy platform regressions therefore
keep their explicit boot configuration.

CMA must be enabled in the kernel and reserved at boot; the host CLI reserves
640 MiB by default. A capacity failure is returned to the launcher, not resolved
by an implicit reboot. For API, SDK, commands and acceptance tests, see
`capstone/runtime/applications.md` in the LLVM checkout.
