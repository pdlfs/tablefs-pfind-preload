# tablefs-pfind-preload
LD_PRELOAD library for running the LANL [parallel_find](https://github.com/mar-file-system/GUFI/blob/parallel_find/src/parallel_find.c) program on the [DeltaFS/TableFS](https://github.com/pdlfs/tablefs).

```
XXXXXXXXX
XX      XX                 XX                  XXXXXXXXXXX
XX       XX                XX                  XX
XX        XX               XX                  XX
XX         XX              XX   XX             XX
XX          XX             XX   XX             XXXXXXXXX
XX           XX  XXXXXXX   XX XXXXXXXXXXXXXXX  XX         XX
XX          XX  XX     XX  XX   XX       XX XX XX      XX
XX         XX  XX       XX XX   XX      XX  XX XX    XX
XX        XX   XXXXXXXXXX  XX   XX     XX   XX XX    XXXXXXXX
XX       XX    XX          XX   XX    XX    XX XX           XX
XX      XX      XX      XX XX   XX X    XX  XX XX         XX
XXXXXXXXX        XXXXXXX   XX    XX        XX  XX      XX
```

This preload library, as well as DeltaFS/TableFS, was developed, in part, under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. Please see the accompanying LICENSE.txt for further information.

# User Manual

**First, secure a Linux box, install gcc, make, and cmake on it, and then create the following 3 directories.**

* `tablefs-src` (e.g.: $HOME/tablefs-src):

This is for downloading, compiling, and building tablefs and preload code.

* `tablefs-dst` (e.g.: $HOME/tablefs-dst):

This is for installing the artifacts (libs and bins) that we make from the code.
  
* `tablefs-dat` (e.g.: /mount/nvme/tablefs-dat):

This is for storing the tablefs db data during test runs.

**Next, we get the tablefs and preload code and compile it.**

* For tablefs:

```bash
cd ${tablefs-src}
git clone https://github.com/pdlfs/tablefs.git
cd tablefs
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=${tablefs-dst} -DTABLEFS_COMMON_INTREE=ON -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=ON ..
make
make install
```

* For tablefs preload:

```bash
cd ${tablefs-src}
git clone https://github.com/pdlfs/tablefs-pfind-preload.git
cd tablefs-pfind-preload
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=${tablefs-dst} ..
make
make install
```

**After that, we use the `fsmaker` program that we just installed to create a simple tablefs namespace.**
No need to use LD_PRELOAD at this moment.

```bash
cd ${tablefs-dst}/bin
./fsmaker ${tablefs-dat}
```

After running it, goto `tablefs-dat` (`dat`, *not* `dst`) and check if it looks like the following:

```bash
total 12K
-rw-r--r-- 1 qingzhen TableFS   0 Dec 11 16:12 000006.log
-rw-r--r-- 1 qingzhen TableFS 495 Dec 11 16:12 000007.ldb
-rw-r--r-- 1 qingzhen TableFS  16 Dec 11 16:12 CURRENT
-rw-r--r-- 1 qingzhen TableFS   0 Dec 11 16:12 LOG
-rw-r--r-- 1 qingzhen TableFS  99 Dec 11 16:12 MANIFEST-000004
```

**Finally, let's do a LANL/parallel_find run on the tablefs namespace that we just populated.**

We need to use LD_PRELOAD this time. The preload lib is located at `tablefs-dst/lib/libtablefs-pfind-preload.so`. The preload lib needs to know where we stored the tablefs data. We inform it by setting env `PRELOAD_Tablefs_home` to `tablefs-dat`. Then, the preload lib needs to know whether tablefs should be opened readonly. We do this by setting env `PRELOAD_Tablefs_readonly` to `1` or `0` depending on our needs. Since LANL/parallel_find only reads information from a filesystem, we set it to `1`.

The preload lib works by redirecting all filesystem calls whose path starts with `/tablefs` to tablefs. Redirected fs calls will have their path prefix `/tablefs` removed. For example, if parallel_find makes an `opendir` call to `/tablefs/1`,  this call will end up becoming an `opendir` call to `/1` in tablefs.

Now let's do a run.

```bash
env PRELOAD_Tablefs_home=${tablefs-dat} PRELOAD_Tablefs_readonly=1 LD_PRELOAD=${tablefs-dst}/lib/libtablefs-pfind-preload.so /path/to/lanl/gufi/parallel_find /tablefs -n 2
```
