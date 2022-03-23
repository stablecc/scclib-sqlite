# scclib sqlite library

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

Source:
* [BSD 3-Clause License](LICENSE)
* [sqlite](sqlite.txt)