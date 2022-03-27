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
#include <sqlite3.h>
#include <gtest/gtest.h>
#include <string>
#include <system_error>
#include <iostream>
#include <vector>
#include <map>
#include <util/fs.h>

/** \addtogroup sqlite
	@{ */
/** Direct call tests for \ref sqlite \file */
/** \example sqlite/unittest/sqld.cc */
/** @} */

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::system_error;
using fs = scc::util::Filesystem;

struct SqliteTest : public testing::Test
{
	string curdir;
	sqlite3 *db;

	SqliteTest() : db(nullptr)
	{
		curdir = fs::get_current_dir();

		system_error err;
		fs::remove_all("sandbox", &err);
		fs::create_dir("sandbox");
		fs::change_dir("sandbox");
	}
	virtual ~SqliteTest()
	{
		close();

		fs::change_dir(curdir);
		system_error err;
		fs::remove_all("sandbox", &err);
	}
	void close()
	{
		if (db)
		{
			sqlite3_close(db);
			db = nullptr;
		}
	}
	void open(const string& name)
	{
		close();
		int ret = sqlite3_open(name.c_str(), &db);
		cout << "open " << name << ": " << (ret == SQLITE_OK ? "OK" : sqlite3_errstr(ret)) << endl;
		ASSERT_EQ(ret, SQLITE_OK);
	}

	map<string, vector<string>> resm;

	static int callback(void* cls, int cols, char **col_text, char **col_name)
	{
		SqliteTest *me = reinterpret_cast<SqliteTest*>(cls);

		for (int i = 0; i < cols; i++)
		{
			me->resm[col_name[i]].push_back(col_text[i]);
		}
		
		return 0;
	}

	void exec(const string& sql)
	{
		resm.clear();
		int ret = sqlite3_exec(db, sql.c_str(), SqliteTest::callback, this, nullptr);
		cout << "exec \"" << sql.substr(0, 20) << "...\": " << (ret == SQLITE_OK ? "OK" : sqlite3_errstr(ret)) << endl;
		results();
	}

	void results()
	{
		cout << "results: " << endl;
		for (auto& r : resm)
		{
			cout << r.first << ": ";
			for (auto& n : r.second)
			{
				cout << n << " ";
			}
			cout << endl;
		}
	}

	void test_table()
	{
		exec("create table tbl1(one text, two int)");
		ASSERT_EQ(resm.size(), 0);
		exec(
			"begin;"
			"insert into tbl1 values('hello!', 10);"
			"insert into tbl1 values('goodbye', 20);"
			"commit;"
		);
		ASSERT_EQ(resm.size(), 0);
	}

	void valid_table()
	{
		ASSERT_EQ(resm.size(), 2);
		ASSERT_NE(resm.find("one"), resm.end());
		ASSERT_EQ(resm["one"].size(), 2);
		ASSERT_EQ(resm["one"][0], "hello!");
		ASSERT_EQ(resm["one"][1], "goodbye");
		ASSERT_NE(resm.find("two"), resm.end());
		ASSERT_EQ(resm["two"].size(), 2);
		ASSERT_EQ(resm["two"][0], "10");
		ASSERT_EQ(resm["two"][1], "20");
	}
};

TEST_F(SqliteTest, memory_open)
{
	open("file:memdb1?mode=memory&cache=shared");
	for (auto& d : fs::scan_dir("."))
	{
		cout << d.first << " " << d.second << endl;
	}
	system_error err;
	ASSERT_TRUE(fs::file_stat("memdb1", &err).type == scc::util::FileType::unknown);
}

TEST_F(SqliteTest, file_open)
{
	open("file:filedb1?mode=rwc&cache=shared");
	for (auto& d : fs::scan_dir("."))
	{
		cout << d.first << " " << d.second << endl;
	}
	system_error err;
	ASSERT_TRUE(fs::file_stat("filedb1", &err).type == scc::util::FileType::reg);
}

TEST_F(SqliteTest, memory_simple)
{
	open("file:memdb1?mode=memory&cache=shared");
	test_table();
	exec("select * from tbl1;");
	valid_table();
}

TEST_F(SqliteTest, file_simple)
{
	open("file:filedb1?mode=rwc&cache=shared");
	test_table();
	exec("select * from tbl1;");
	valid_table();
}

TEST_F(SqliteTest, file_readback)
{
	open("file:filedb1?mode=rwc&cache=shared");
	test_table();
	exec("select * from tbl1;");
	valid_table();

	open("file:filedb1?mode=ro&cache=shared");
	exec("select * from tbl1;");
	valid_table();
}
