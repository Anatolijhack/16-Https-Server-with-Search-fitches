#pragma once
#include "CartRepository.h"
#include "Router.h"

class CartController
{
public:
    CartController(ConnectionPool& pool) : cart_repository(pool) {}

    void register_routes(Router& router);

private:
    Response add_item(const Request& req);
    Response get_cart(const Request& req);
    Response update_item(const Request& req);
    Response remove_item(const Request& req);

    CartRepository cart_repository;
};