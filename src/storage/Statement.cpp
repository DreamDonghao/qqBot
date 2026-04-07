/// @file Statement.cpp
/// @brief SQLite Statement RAII 封装 - 实现
/// @author donghao
/// @date 2026-04-02

#include <storage/Statement.hpp>

namespace LittleMeowBot {
    Statement::Statement(sqlite3* db, std::string_view sql) : m_db(db){
        if (sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &m_stmt, nullptr) != SQLITE_OK) {
            std::string err = sqlite3_errmsg(db);
            spdlog::error("SQL 准备失败: {} - {}", sql, err);
            throw DbError(err);
        }
    }

    Statement::~Statement(){
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
        }
    }

    Statement::Statement(Statement&& other) noexcept
        : m_db(std::exchange(other.m_db, nullptr))
          , m_stmt(std::exchange(other.m_stmt, nullptr)){}

    Statement& Statement::operator=(Statement&& other) noexcept{
        if (this != &other) {
            if (m_stmt) sqlite3_finalize(m_stmt);
            m_db = std::exchange(other.m_db, nullptr);
            m_stmt = std::exchange(other.m_stmt, nullptr);
        }
        return *this;
    }

    bool Statement::step() noexcept{
        int rc = sqlite3_step(m_stmt);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        spdlog::error("SQL 执行失败: {}", sqlite3_errmsg(m_db));
        return false;
    }

    void Statement::exec() noexcept{
        sqlite3_step(m_stmt);
    }

    void Statement::reset() noexcept{
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }

    Transaction::Transaction(sqlite3* db) : m_db(db){
        sqlite3_exec(m_db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    }

    Transaction::~Transaction(){
        if (!m_committed) {
            sqlite3_exec(m_db, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    void Transaction::commit() noexcept{
        sqlite3_exec(m_db, "COMMIT", nullptr, nullptr, nullptr);
        m_committed = true;
    }
} // namespace LittleMeowBot