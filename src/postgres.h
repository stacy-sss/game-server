#pragma once
#include <pqxx/pqxx>
#include "application.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cassert>

namespace postgres {
	class ConnectionPool {
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

public:
    class ConnectionWrapper {
    public:
        ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection() {
        std::unique_lock lock{mutex_};
        // Ѕлокируем текущий поток и ждЄм, пока cond_var_ не получит уведомление и не освободитс€
        // хот€ бы одно соединение
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
        });
        // ѕосле выхода из цикла ожидани€ мьютекс остаЄтс€ захваченным

        return {std::move(pool_[used_connections_++]), *this};
    }

private:
    void ReturnConnection(ConnectionPtr&& conn) {
        // ¬озвращаем соединение обратно в пул
        {
            std::lock_guard lock{mutex_};
            assert(used_connections_ != 0);
            pool_[--used_connections_] = std::move(conn);
        }
        // ”ведомл€ем один из ожидающих потоков об изменении состо€ни€ пула
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};


class Database{
public:
    explicit Database(const std::string& db_url, size_t pool_size);//соедин€ем с пулом строим таблицы в релизации конструктора
    ~Database() = default;
    void InitSchema();
    void AddRetiredPlayer(const application::RetiredPlayer& player);
    std::vector<application::RetiredPlayer> GetRecords(int start, int max_items);
private:
    ConnectionPool pool_;
};
}