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
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <cassert>

/** \addtogroup sqlite
	@{ */
/** \ref sqlite implementation \file */
/** @} */

static const std::string::size_type npos = std::string::npos;

using namespace scc::sqld;

Conn::Conn(const std::string& uri) : m_db(nullptr)
{
	int r = sqlite3_open(uri.c_str(), &m_db);
	if (r != SQLITE_OK)
	{
		throw std::runtime_error(sqlite3_errstr(r));
	}
}

Conn::~Conn()
{
	close();
}

void Conn::close()
{
	if (m_db)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

void Conn::reopen(const std::string& uri)
{
	close();

	int r = sqlite3_open(uri.c_str(), &m_db);
	if (r != SQLITE_OK)
	{
		throw std::runtime_error(sqlite3_errstr(r));
	}
}

Trans::Trans(Conn& conn) : m_conn(conn), m_active(false)
{
}

Trans::~Trans()
{
	if (m_active)
	{
		abort();
	}
}

void Trans::begin()
{
	if (m_active)
	{
		throw std::runtime_error("begin() transaction when already active");
	}
	Req r(m_conn);
	r.sql() << "BEGIN;";
	r.exec();
	m_active = true;
}

void Trans::commit()
{
	if (!m_active)
	{
		throw std::runtime_error("commit() transaction when not active");
	}
	Req r(m_conn);
	r.sql() << "COMMIT;";
	r.exec();
	m_active = false;
}

void Trans::abort()
{
	if (!m_active)
	{
		throw std::runtime_error("abort() transaction when not active");
	}
	Req r(m_conn);
	r.sql() << "ROLLBACK;";
	r.exec();
	m_active = false;
}

Req::Req(Conn& conn) : m_conn(conn), m_stmt(nullptr), m_pos(0), m_cols(0)
{
}

Req::~Req()
{
	finalize();
}

void Req::finalize()
{
	if (m_stmt)
	{
		sqlite3_finalize(m_stmt);
		m_stmt = nullptr;
	}
}

void Req::clear()
{
	finalize();

	m_sql.str("");		// clear out the statement
	m_pos = 0;
	m_cols = 0;
}

void Req::reset()
{
	finalize();

	m_pos = 0;
	m_cols = 0;
}

//#include <iostream>

void Req::prepare()
{
	finalize();								// clean up the last statement if any

	if (m_pos >= m_sql.str().size()) 		// nothing to execute
	{
		return;
	}

//std::cout << "prepare: " << m_sql.str().substr(m_pos) << std::endl;

	/*
		The sqlite library compiles one statement at a time, and keeps track of where it left off,
		so start at the current position.
	*/
	const char *head = &(m_sql.str()[m_pos]), *tail = nullptr;

	int r = sqlite3_prepare_v2(m_conn.m_db, head, m_sql.str().size()+1, &m_stmt, &tail);		// include 0 character
	if (r != SQLITE_OK)
	{
		throw std::runtime_error(sqlite3_errstr(r));
	}

	assert(tail);
	m_pos += tail-head;	// advance (when there are no more statements this will be larger than the sql string)
}

int Req::exec_select()
{
	if (m_cols)
	{
		throw std::runtime_error("exec_select() called with current row data");
	}

	while (1)
	{
		prepare();			// go process the next request in the sql stream
		
		if (!m_stmt)		// this happens when we are done processing or there is only whitespace left
		{
			return 0;
		}

		int r = sqlite3_step(m_stmt);

		if (r == SQLITE_DONE)	// no row data, go on to next statement
		{
			continue;
		}

		if (r != SQLITE_ROW)
		{
			throw std::runtime_error(sqlite3_errstr(r));
		}

		m_cols = sqlite3_column_count(m_stmt);		// we have row data, keep track of number of columns in row

		break;
	}

	return m_cols;
}

void Req::exec()
{
	if (m_cols)
	{
		throw std::runtime_error("exec() called with current row data");
	}

	while (1)
	{
		int r = exec_select();

		if (r == 0)		// done
		{
			return;
		}

		while (next_row())	// keep going until we have no more row data
		{
		}

		// let the exec_select run again to make sure there are no more statements
	}
}

int Req::next_row()
{
	if (!m_stmt)
	{
		throw std::runtime_error("next_row() called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("next_row() called without current row data");
	}

	int r = sqlite3_step(m_stmt);

	if (r == SQLITE_DONE)			// no more columns
	{
		m_cols = 0;
		return 0;
	}

	if (r != SQLITE_ROW)			// another column coming
	{
		throw std::runtime_error(sqlite3_errstr(r));
	}

	return sqlite3_column_count(m_stmt);
}

std::string Req::col_name(int col)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	return sqlite3_column_name(m_stmt, col);
}

void Req::col_name(int col, std::string& str)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	str.assign(sqlite3_column_name(m_stmt, col));
}

std::string Req::col_text(int col)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	return reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, col));
}

void Req::col_text(int col, std::string& str)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	str.assign(reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, col)));
}

int Req::col_int(int col)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	return sqlite3_column_int(m_stmt, col);
}

int64_t Req::col_int64(int col)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	return sqlite3_column_int64(m_stmt, col);
}

double Req::col_real(int col)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}

	return sqlite3_column_double(m_stmt, col);
}

void Req::col_blob(int col, std::vector<char>& str)
{
	if (!m_stmt)
	{
		throw std::runtime_error("column operation called with invalid statement");
	}
	if (!m_cols)
	{
		throw std::runtime_error("column operation called when row not available");
	}
	if (col < 0 || col >= m_cols)
	{
		throw std::runtime_error("column operation called with invalid column number");
	}
	
	int sz = sqlite3_column_bytes(m_stmt, col);

	const char* v = reinterpret_cast<const char*>(sqlite3_column_blob(m_stmt, col));

	str.assign(v, v+sz);
}
