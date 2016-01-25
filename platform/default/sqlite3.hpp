#pragma once

#include <string>
#include <stdexcept>
#include <mbgl/storage/response.hpp>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

namespace mapbox {
namespace sqlite {

enum OpenFlag : int {
    ReadOnly = 0x00000001,
    ReadWrite = 0x00000002,
    Create = 0x00000004,
    NoMutex = 0x00008000,
    FullMutex = 0x00010000,
    SharedCache = 0x00020000,
    PrivateCache = 0x00040000,
};

struct Exception : std::runtime_error {
    inline Exception(int err, const char *msg) : std::runtime_error(msg), code(err) {}
    inline Exception(int err, const std::string& msg) : std::runtime_error(msg), code(err) {}
    const int code = 0;
};

class Statement;
    
class Database {
private:
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

public:
    Database(const std::string &filename, int flags = 0);
    Database(Database &&);
    ~Database();
    Database &operator=(Database &&);

    operator bool() const;

    void exec(const std::string &sql);
    Statement prepare(const char *query);
    bool ensureSchemaVersion(const int schemaVersion, const std::string &tableName);
    
    //Retrieves cached data for a query.  The data is assumed to be a blob, and is assumed to be in
    //column 0.  If compressed is true, the data is compressed/uncompressed.  If the data is missing,
    //retrieve is called, and if it returns true, the query is run again.  If anything goes wrong,
    //false is returned and the response is called with an error code.
    void retrieveCachedData(const Statement &query,
                            bool compressed,
                            std::function<void (mbgl::Response)> callback,
                            std::function<void (std::function<void (void)>)> retrieve);

    
    
private:
    sqlite3 *db = nullptr;
};
    
bool database_createSchema(std::shared_ptr<Database> db,
                           const std::string &path,
                           const char *const sql,
                           const int schemaVersion,
                           const std::string &tableName);
    
    
class Statement {
private:
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;

    void check(int err);

public:
    Statement(sqlite3 *db, const char *sql);
    Statement(Statement &&);
    ~Statement();
    Statement &operator=(Statement &&);

    operator bool() const;

    template <typename T> void bind(int offset, T value);
    void bind(int offset, const std::string &value, bool retain = true);
    template <typename T> T get(int offset) const;

    bool run(bool expectResults = true) const;
    void reset();

private:
    sqlite3_stmt *stmt = nullptr;
};

}
}
