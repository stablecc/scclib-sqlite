\mainpage sqlite documentation
# scclib sqlite library

Provides c++ classes and library for [sqlite](https://www.sqlite.org/index.html).

## sqlite c++ library

Current documentation is available on
[stablecc.github.io](https://stablecc.github.io/scclib-sqlite/).

## sqlite source

`sqlite3.h` and `sqlite3.c` are amalgamated [sqlite](https://github.com/sqlite/sqlite)
source code, produced as follows:
```
wget https://github.com/sqlite/sqlite/archive/refs/tags/version-3.37.2.tar.gz
tar xf version-3.37.2.tar.gz
mkdir sqlite
cd sqlite
../sqlite-version-3.37.2/configure
make sqlite3.c
```

## licensing

Original source:
* [BSD 3-Clause License](lic/bsd_3_clause.txt)

External and redistributable:
* [sqlite](lic/sqlite.txt)
