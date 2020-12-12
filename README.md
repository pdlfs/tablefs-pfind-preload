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

# User Manual (LANL/parallel_find)

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
rm -rf ${tablefs-dat}   # clean up data of previous runs
mkdir -p ${tablefs-dat}   # ensure parent directories
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

Now let's do a run with 1 parallel_find thread.

```bash
env PRELOAD_Tablefs_home=${tablefs-dat} PRELOAD_Tablefs_readonly=1 LD_PRELOAD=${tablefs-dst}/lib/libtablefs-pfind-preload.so /path/to/lanl/gufi/parallel_find /tablefs -n 1
```

Here's its output.

```bash
/tablefs
/tablefs/1
/tablefs/1/a
/tablefs/1/b
/tablefs/1/c
/tablefs/2
/tablefs/2/a
/tablefs/2/b
/tablefs/2/c
/tablefs/3
/tablefs/3/a
/tablefs/3/b
/tablefs/3/c
Bye
```

Now, let's do a run with 2 parallel_find threads.

```bash
env PRELOAD_Tablefs_home=${tablefs-dat} PRELOAD_Tablefs_readonly=1 LD_PRELOAD=${tablefs-dst}/lib/libtablefs-pfind-preload.so /path/to/lanl/gufi/parallel_find /tablefs -n 2
```

Here's the output. Note that this time the directories are printed in random order, meaning that everything works as expected.

```bash
/tablefs
/tablefs/1
/tablefs/2
/tablefs/1/a
/tablefs/1/b
/tablefs/1/c
/tablefs/3
/tablefs/2/a
/tablefs/2/b
/tablefs/2/c
/tablefs/3/a
/tablefs/3/b
/tablefs/3/c
Bye
```

If we don't like the `/tablefs` path prefix we can change it by setting env `PRELOAD_Tablefs_path_prefix` to other prefixes. When we do that, remember to invoke parallel_find accordingly for calls to be properly redirected.

# User Manual (ior/mdtest)

* MPICH

```bash
rm -rf ${tablefs-dat}   # clean up data of previous runs
mkdir -p ${tablefs-dat}   # ensure parent directories
mpirun -np 1 -env PRELOAD_Tablefs_home ${tablefs-dat} -env LD_PRELOAD ${tablefs-dst}/lib/libtablefs-pfind-preload.so \
  /path/to/hpc/ior/mdtest -C -T -k -n 40 -z 3 -b 3 -d /tablefs
```

* OpenMPI

```bash
rm -rf ${tablefs-dat}   # clean up data of previous runs
mkdir -p ${tablefs-dat}   # ensure parent directories
mpirun -np 1 -x PRELOAD_Tablefs_home=${tablefs-dat} -x LD_PRELOAD=${tablefs-dst}/lib/libtablefs-pfind-preload.so \
  /path/to/hpc/ior/mdtest -C -T -k -n 40 -z 3 -b 3 -d /tablefs
```

Here's its output.

```bash
-- started at 12/11/2020 23:23:21 --

mdtest-3.4.0+dev was launched with 1 total task(s) on 1 node(s)
Command line used: ./mdtest '-C' '-T' '-k' '-n' '40' '-z' '3' '-b' '3' '-d' '/tablefs'
POSIX couldn't call statvfs: No such file or directory
WARNING: Backend returned error during statfs.
Nodemap: 1
1 tasks, 40 files/directories

SUMMARY rate: (of 1 iterations)
   Operation                      Max            Min           Mean        Std Dev
   ---------                      ---            ---           ----        -------
   Directory creation        :      18094.602      18094.602      18094.602          0.000
   Directory stat            :      32452.966      32452.966      32452.966          0.000
   Directory removal         :          0.000          0.000          0.000          0.000
   File creation             :      17605.130      17605.130      17605.130          0.000
   File stat                 :      27030.881      27030.881      27030.881          0.000
   File read                 :          0.000          0.000          0.000          0.000
   File removal              :          0.000          0.000          0.000          0.000
   Tree creation             :      21622.253      21622.253      21622.253          0.000
   Tree removal              :          0.000          0.000          0.000          0.000
-- finished at 12/11/2020 23:23:21 --
```
