#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "ConnectionPool.h"
//#include <windows.h> 
//#include <sql.h>
//#include <sqlext.h>
//struct databaseconnection
//{
//    sqlhenv env = nullptr;
//    sqlhdbc dbc = nullptr;
//};
struct ProductRecord
{
    int id;
    std::string type;
    std::string product_name;
    double price;
    int stock_quantity;
};
struct ProductQuery
{
    std::optional<std::string> search;
    std::optional<std::string> type;
    std::string sort_by = "id";      // id | price
    std::string sort_dir = "ASC";    // ASC | DESC
    int limit = 20;
    int offset = 0;
};
class ProductRepository
{
public:
    ProductRepository(ConnectionPool& pool) : pool(pool) {}

    std::string get_products();
    std::string get_product(int id);
    std::string add_product(const std::string& type, const std::string& name, double cost, double price, int stock);
    std::string update_product(int id, double price, int stock);
    std::string delete_product(int id);
    std::string get_products_by_type(const std::string& type);
    std::string search_products(const ProductQuery& q);


private:
    ConnectionPool& pool;
};