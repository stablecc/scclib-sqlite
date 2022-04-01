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
#ifndef _SCC_SQLD_H
#define _SCC_SQLD_H

#include <string>
#include <sstream>
#include <vector>

struct sqlite3;		// forward declaration
struct sqlite3_stmt;

namespace scc::sqld
{

/** \addtogroup sqlite
	@{
*/

/** File descriptor.
	\file
*/

/** Database connection.

	Uses the [uri method](https://sqlite.org/uri.html) to specifiy a connection.

	The default is a [shared cache](https://sqlite.org/sharedcache.html) connection
	to an [in-memory](https://sqlite.org/inmemorydb.html) database.

	Once constructed or reopened, a database connection is thread-safe.
*/
class Conn
{
	sqlite3* m_db;
	friend class Req;

	void close();
public:
	/** Constructs and open a sqlite in-memory database connection.
	*/
	Conn(const std::string& = "file:mem?mode=memory&cache=shared");
	virtual ~Conn();

	Conn(const Conn&) = delete;
	Conn& operator=(const Conn&) = delete;
	Conn(Conn&&) = delete;
	Conn& operator=(Conn&&) = delete;

	/** Reopen the connection.

		The database will be destroyed and reopened. This command is not thread-safe.
	*/
	void reopen(const std::string& = "file:mem?mode=memory&cache=shared");
};

/** Database transaction.

	An active transaction will be aborted when the object is destroyed.
*/
class Trans
{
	Conn& m_conn;
	bool m_active;
public:
	Trans(Conn&);
	virtual ~Trans();

	/** BEGIN the transaction.

		Throws an exception if the transaction is already active.
	*/
	void begin();
	
	/** COMMIT the transaction.

		Throws an exception if the transaction is not active.
	*/
	void commit();
	
	/** ROLLBACK (abort) the transaction.

		Throws an exception if the transaction is not active.
	*/
	void abort();

	/** Is this transaction active? */
	bool is_active() const { return m_active; }
};

/** Database request.

	Sqlite [data types](https://www.sqlite.org/datatype3.html) are more flexible than standard databases,
	in general the types are automatically detected.

	Information on the recent STRICT table option is [here](https://www.sqlite.org/stricttables.html).

	UTF-16 values are not implemented.
*/
class Req
{
	Conn& m_conn;
	sqlite3_stmt* m_stmt;

	std::ostringstream m_sql;
	std::string::size_type m_pos;
	int m_cols;

	void finalize();
	void prepare();
public:
	Req(Conn&);
	virtual ~Req();

	/** Clear the request and the sql() stream.

		After this call, the sql() stream is set empty, and the request is initialized.
	*/
	void clear();

	/** Reset the request without clearing the sql() stream.

		After this call, the sql() stream is unchanged, and the request is initialized to start of stream.
		The sql stream can then be executed again.
	*/
	void reset();

	/** Executes in select mode.

		Executes statements in sql() stream, until either row data is available, or there are no more
		statements to process.
		
		If row data is returned, the caller must call next_row() until all rows are
		retrieved (or clear the request).

		\returns Number of columns in the current row, or 0 if no row data.
	*/
	int exec_select();

	/** Get the next row.

		If there are multiple select-type statements in the sql() stream, exec_select() may need to be called
		again after next_row() returns 0.

		\returns Number of columns in the current row, or 0 if no row data.
	*/
	int next_row();

	/** Execute all statements, ignoring all row data.
	*/
	void exec();

	/** Return column name.
		\param col zero-indexed column
	*/
	std::string col_name(int);
	
	/** Return column name.
		\param col zero-indexed column
	*/
	void col_name(int, std::string&);

	/** Return UTF-8 TEXT.
		\param col zero-indexed column
	*/
	std::string col_text(int);
	
	/** Return UTF-8 TEXT.
		\param col zero-indexed column
	*/
	void col_text(int, std::string&);

	/** Return 32-bit INTEGER.
		\param col zero-indexed column
	*/
	int col_int(int);

	/** Return 64-bit INTEGER.
		\param col zero-indexed column
	*/
	int64_t col_int64(int);

	/** Return 64-bit REAL.
		\param col zero-indexed column
	*/
	double col_real(int);

	/** Return BLOB unstructured data.
		\param col zero-indexed column
	*/
	void col_blob(int, std::vector<char>&);

	/** Sql streamer.

		Adds to the request, for example:

		    req.sql()
		      << "create table t(one int, two int);"
		      << "insert into t values(1, 2);"
		      << "select * from t;";
	*/
	std::ostringstream& sql()
	{
		return m_sql;
	}
};

/** @} */
}

#endif
