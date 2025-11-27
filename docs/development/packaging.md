# Packaging

----

To create deb package for local or test-env use, you can do `cpack-*` targets from Makefile.
Packages will be created at `build/packages` directory.
```sh
make cpack-deb

$ ls build/packages
odyssey-dbgsym_1.4rc-2276-d458212f_amd64.ddeb odyssey_1.4rc-2276-d458212f_amd64.deb
```

For more convenient way to build package, consider use `dpkg-buildpackage`. Some example can be run by `package-*` targets:
```sh
make package-bionic

$ ls build/packages
odyssey-dbg_1.3-200_amd64.deb  odyssey_1.3-200.dsc  odyssey_1.3-200.tar.xz  odyssey_1.3-200_amd64.buildinfo  odyssey_1.3-200_amd64.changes  odyssey_1.3-200_amd64.deb
```
