/*
 * Copyright (c) 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#ifndef __BASE_MYSQL_HH__
#define __BASE_MYSQL_HH__

#define TO_BE_INCLUDED_LATER 0

#include <cassert>
#include <iosfwd>
#include <mysql_version.h>
#include <mysql.h>
#include <string>
#include <sstream>

namespace MySQL {

class Error
{
  protected:
    const char *error;

  public:
    Error() : error(NULL) {}

    Error &clear() { error = NULL; return *this; }
    Error &set(const char *err) { error = err; return *this; }

    const char *string() const { return error; }

    operator bool() const { return error != NULL; }
    bool operator!() const { return error == NULL; }
};

std::ostream &operator<<(std::ostream &stream, const Error &error);

class Result
{
  private:
    MYSQL_RES *result;
    int *refcount;

    void
    decref()
    {
	if (!refcount)
	    return;

	*refcount -= 1;
	if (*refcount == 0) {
	    mysql_free_result(result);
	    delete refcount;
	}

	refcount = NULL;
    }
	
  public:
    Result()
	: result(0), refcount(NULL)
    { }

    Result(MYSQL_RES *res)
	: result(res)
    {
	if (result)
	    refcount = new int(1);
        else
            refcount = NULL;
    }

    Result(const Result &result)
	: result(result.result), refcount(result.refcount)
    {
	if (result)
	    *refcount += 1;
    }

    ~Result()
    {
	decref();
    }

    const Result &
    operator=(MYSQL_RES *res)
    {
	decref();
	result = res;
	if (result)
	    refcount = new int(1);

	return *this;
    }
	    
    const Result &
    operator=(const Result &res)
    {
	decref();
	result = res.result;
	refcount = res.refcount;
	if (result)
	    *refcount += 1;

	return *this;
    }

    operator bool() const { return result != NULL; }
    bool operator!() const { return result == NULL; }

    unsigned
    num_fields()
    {
	assert(result);
	return mysql_num_fields(result);
    }

    MYSQL_ROW
    fetch_row()
    {
	return mysql_fetch_row(result);
    }

    unsigned long *
    fetch_lengths()
    {
	return mysql_fetch_lengths(result);
    }
};

typedef MYSQL_ROW Row;

class Connection
{
  protected:
    MYSQL mysql;
    bool valid;

  protected:
    std::string _host;
    std::string _user;
    std::string _passwd;
    std::string _database;
    
  public:
    Connection();
    virtual ~Connection();

    bool connected() const { return valid; }
    bool connect(const std::string &host, const std::string &user,
		 const std::string &passwd, const std::string &database);
    void close();

  public:
    Error error;
    operator MYSQL *() { return &mysql; }

  public:
    bool query(const std::string &sql);

    bool
    query(const std::stringstream &sql)
    {
	return query(sql.str());
    }

    bool
    autocommit(bool mode)
    {
        return mysql_autocommit(&mysql, mode);
    }

    bool
    commit()
    {
        return mysql_commit(&mysql);
    }

    bool
    rollback()
    {
        return mysql_rollback(&mysql);
    }

    unsigned
    field_count()
    {
	return mysql_field_count(&mysql);
    }

    unsigned
    affected_rows()
    {
	return mysql_affected_rows(&mysql);
    }

    unsigned
    insert_id()
    {
	return mysql_insert_id(&mysql);
    }


    Result
    store_result()
    {
	error.clear();
	Result result = mysql_store_result(&mysql);
	if (!result)
	    error.set(mysql_error(&mysql));

	return result;
    }
};

#if 0
class BindProxy
{
    MYSQL_BIND *bind;
    BindProxy(MYSQL_BIND *b) : bind(b) {}

    void operator=(bool &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_TINY;
	bind->buffer = (char *)&buffer;
    }

    void operator=(int8_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_TINY;
	bind->buffer = (char *)&buffer;
    }

    void operator=(int16_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_SHORT;
	bind->buffer = (char *)&buffer;
    }

    void operator=(int32_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_LONG;
	bind->buffer = (char *)&buffer;
    }

    void operator=(int64_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_LONGLONG;
	bind->buffer = (char *)&buffer;
    }

    void operator=(uint8_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_TINY;
	bind->buffer = (char *)&buffer;
    }

    void operator=(uint16_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_SHORT;
	bind->buffer = (char *)&buffer;
    }

    void operator=(uint32_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_LONG;
	bind->buffer = (char *)&buffer;
    }

    void operator=(uint64_t &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_LONGLONG;
	bind->buffer = (char *)&buffer;
    }

    void operator=(float &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_FLOAT;
	bind->buffer = (char *)&buffer;
    }

    void operator=(double &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_DOUBLE;
	bind->buffer = (char *)&buffer;
    }

    void operator=(Time &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_DATE;
	bind->buffer = (char *)&buffer;
    }

    void operator=(const char *buffer)
    {
	bind->buffer_type = MYSQL_TYPE_VAR_STRING;
	bind->buffer = buffer;
    }

    void operator=(const std::string &buffer)
    {
	bind->buffer_type = MYSQL_TYPE_VAR_STRING;
	bind->buffer = (char *)&buffer;
	bind->length = buffer.length;
    }

    bool
    set_null(bool null)
    {
	bind->is_null = null;
    }
};

class Statement
{
  protected:
    Error &error;
    MYSQL_STMT *stmt;
    MYSQL_BIND *bind;
    int size;

  public:
    Statement(Connection &mysql)
	: error(mysql.error), bind(NULL), size(0)
    {
	stmt = mysql_stmt_init(mysql);
	assert(valid() && "mysql_stmt_init(), out of memory\n");
    }

    ~Statement()
    {
	assert(valid());
	error.clear();
	if (mysql_stmt_close(stmt))
	    error.set(mysql_stmt_error(stmt));

	if (bind)
	    delete [] bind;
    }

    bool valid()
    {
	return stmt != NULL;
    }

    void prepare(const std::string &query)
    {
	assert(valid());
	mysql.error.clear();
	if (mysql_stmt_prepare(mysql, query, strlen(query)))
	    mysql.error.set(mysql_stmt_error(stmt));

	int size = count();
	bind = new MYSQL_BIND[size];
    }

    unsigned count()
    {
	assert(valid());
	return mysql_stmt_param_count(stmt);
    }

    unsigned affected()
    {
	assert(valid());
	return mysql_stmt_affected_rows(stmt);
    }

    void bind(MYSQL_BIND *bind)
    {
	mysql.error.clear();
	if (mysql_stmt_bind_param(stmt, bind))
	    mysql.error.set(mysql_stmt_error(stmt));
    }

    BindProxy operator[](int index)
    {
	assert(index > 0 && index < N);
	return &bind[N];
    }

    operator MYSQL_BIND *()
    {
	return bind;
    }

    void operator()()
    {
	assert(valid());
	error.clear();
	if (mysql_stmt_execute(stmt))
	    error.set(mysql_stmt_error(stmt));
    }
}
#endif

/* namespace MySQL */ }

#endif // __BASE_MYSQL_HH__
