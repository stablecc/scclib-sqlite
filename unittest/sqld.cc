/*
BSD 3-Clause License

Copyright (c) 2022, Stable Cloud Computing, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <sqlite/sqld.h>
#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <system_error>
#include <util/fs.h>

/** \addtogroup sqlite
	@{ */
/** Tests for \ref sqlite \file */
/** \example unittest/sqld.cc */
/** @} */

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::system_error;
using std::runtime_error;
using fs = scc::util::Filesystem;
using scc::sqld::Conn;
using scc::sqld::Req;
using scc::sqld::Trans;

struct SqliteTest : public testing::Test
{
	string curdir;
	Conn db;

	SqliteTest()
	{
		curdir = fs::get_current_dir();

		system_error err;
		fs::remove_all("sandbox", &err);
		fs::create_dir("sandbox");
		fs::change_dir("sandbox");
	}
	virtual ~SqliteTest()
	{
		fs::change_dir(curdir);
		system_error err;
		fs::remove_all("sandbox", &err);
	}

	void reopen(const string& uri)
	{
		cout << "opening: " << uri << endl;
		db.reopen(uri);
	}
};

TEST_F(SqliteTest, default_conn)
{
	auto fs = fs::scan_dir(".");
	cout << "filesystem:" << endl;
	for (auto& d : fs)
	{
		cout << d.first << " " << d.second << endl;
	}
	ASSERT_EQ(fs.size(), 0);
}

TEST_F(SqliteTest, memory_conn)
{
	reopen("file::memory:");		// reopen the connection without the shared cache
	auto fs = fs::scan_dir(".");
	cout << "filesystem:" << endl;
	for (auto& d : fs)
	{
		cout << d.first << " " << d.second << endl;
	}
	ASSERT_EQ(fs.size(), 0);
}

TEST_F(SqliteTest, file_open)
{
	reopen("file:file?mode=rwc");	// open a read/write/create file backed connection
	auto fs = fs::scan_dir(".");
	cout << "filesystem:" << endl;
	for (auto& d : fs)
	{
		cout << d.first << " " << d.second << endl;
	}
	ASSERT_EQ(fs.size(), 1);
	system_error err;
	ASSERT_TRUE(fs::file_stat("file", &err).type == scc::util::FileType::reg);
}

TEST_F(SqliteTest, exec_select)
{
	Req req(db);

	req.sql()
		<< "create table t(a TEXT, b INT) STRICT;"
		<< "insert into t values('hello!', 1);"
		<< "insert into t values('goodbye', 2);"
		<< "select * from t;";

	int r = req.exec_select();
	ASSERT_EQ(r, 2);

	cout << "first col name: " << req.col_name(0) << endl;
	ASSERT_EQ(req.col_name(0), "a");
	string n;
	req.col_name(1, n);
	cout << "second col name: " << n << endl;
	ASSERT_EQ(n, "b");

	ASSERT_EQ(req.col_text(0), "hello!");
	req.col_text(0, n);
	ASSERT_EQ(n, "hello!");

	ASSERT_EQ(req.col_int(1), 1);

	ASSERT_EQ(req.next_row(), 2);

	ASSERT_EQ(req.col_text(0), "goodbye");
	req.col_text(0, n);
	ASSERT_EQ(n, "goodbye");

	ASSERT_EQ(req.col_int(1), 2);

	ASSERT_EQ(req.next_row(), 0);

	ASSERT_EQ(req.exec_select(), 0);
}

TEST_F(SqliteTest, exec)
{
	reopen("file:file?mode=rwc");
	Req req(db);

	int64_t big = 1;
	big <<= 48;

	cout << "big is " << big << endl;

	req.sql()
		<< "create table t(a VARCHAR PRIMARY KEY, b DOUBLE, c INTEGER DEFAULT 0);"
		<< "insert into t (a, b) values('hello!', 1.1);"
		<< "insert into t values('goodbye', 2.2, " << big << ");"
		<< "select * from t;"
		<< "insert into t (a, b) values('until we meet again', 3.3);";

	req.exec();		// should go through all the statements ignoring the select statement

	req.clear();
	req.sql() << "select b,c from t where a is 'goodbye';";

	int r = req.exec_select();
	ASSERT_EQ(r, 2);
	ASSERT_EQ(req.col_real(0), 2.2);
	ASSERT_EQ(req.col_int64(1), big);
}

TEST_F(SqliteTest, blob)
{
	Req req(db);

	req.sql()
		<< "create table t(a BLOB) STRICT;"
		<< "insert into t values(x'deadbeef');"
		<< "select * from t;";

	int r = req.exec_select();
	ASSERT_EQ(r, 1);

	vector<char> t = {'\xde', '\xad', '\xbe', '\xef'}, v;
	req.col_blob(0, v);
	ASSERT_EQ(t, v);
}

TEST_F(SqliteTest, two_conns_xact)
{
	reopen("file:file?mode=rwc");
	
	Req r(db);

	r.sql()	<< "create table t(a ANY) STRICT;";
	r.exec();										// create the table

	Trans x(db);

	x.begin();										// begin a transaction
	r.clear();
	r.sql()	<< "insert into t values(12345);";
	r.exec();										// insert a value

	Conn db2("file:file?mode=rwc");		// second connection

	Req r2(db2);

	r2.sql() << "select * from t;";

	ASSERT_EQ(r2.exec_select(), 0); 	// cannot see the value yet

	x.commit();							// first connection commits transaction

	r2.reset();

	ASSERT_EQ(r2.exec_select(), 1);		// now the value can be seen

	x.begin();
	r.clear();
	r.sql()	<< "insert into t values(45678);";
	r.exec();
	x.abort();

	r.clear();
	r.sql() << "select * from t where a is 45678;";
	ASSERT_EQ(r.exec_select(), 0);						// it was rolled back
}
