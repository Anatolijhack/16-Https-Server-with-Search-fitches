// AuthController.cpp
#include "AuthController.h"
#include "SessionStore.h"
#include <nlohmann/json.hpp>
#include "PasswordHash.h" 
#include "Validation.h"
#include "AuthMiddleware.h"
using json = nlohmann::json;

void AuthController::register_routes(Router& router)
{
    router.add("POST", "/login", [this](const Request& req)
        {
            return login(req);
        }
    );

    router.add("POST", "/register", [this](const Request& req)
        {
            return register_user(req);
        }
    );

    router.add("POST", "/setup/create-admin", [this](const Request& req)
        {
            return create_admin_setup(req);
        }
    );
    router.add("POST", "/setup/create-worker", [this](const Request& req) {return create_worker_setup(req); });

    router.add("POST", "/logout", [this](const Request& req) { return logout(req); });
    router.add("POST", "/logout/all", [this](const Request& req) { return logout_all(req); });
    router.add("PUT", "/me/password", [this](const Request& req)
        {
            return change_password(req);
        });


}

Response AuthController::login(const Request& req)
{
    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string username, password;

    try
    {
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing username or password"})", "application/json", "400 Bad Request" };
    }

    auto user = user_repository.find_by_username(username);

    if (!user)
    {
        std::cout << "USER NOT FOUND: " << username << std::endl;

        return Response{
            R"({"error":"Invalid credentials"})",
            "application/json",
            "401 Unauthorized"
        };
    }

    std::cout << "USER FOUND" << std::endl;
    std::cout << "id: " << user->id << std::endl;
    std::cout << "username: " << user->username << std::endl;
    std::cout << "role: " << user->role << std::endl;
    std::cout << "hash: " << user->password_hash << std::endl;

    bool password_ok =
        PasswordHash::verify_password(password, user->password_hash);

    std::cout << "PASSWORD OK: " << password_ok << std::endl;

    if (!password_ok)
    {
        return Response{
            R"({"error":"Invalid credentials"})",
            "application/json",
            "401 Unauthorized"
        };
    }

    std::string token = SessionStore::instance().create_token(user->id,user->username, user->role);

    json response = {
    {"token", token},
    {"role", user->role},
    {"user_id", user->id},
    {"expires_in", SessionStore::instance().idle_seconds()}
    };

    return Response{ response.dump(), "application/json" };
}



Response AuthController::register_user(const Request& req)
{
    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string username, password;
    std::string role = "customer"; // фиксированна€ роль дл€ саморегистрации

    try
    {
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing username or password"})", "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_username(username))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_password(password))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    std::string password_hash = PasswordHash::hash_password(password);

    std::string result = user_repository.create_user(username, password_hash, role);
    json result_json = json::parse(result);

    std::string status_code = "201 Created";
    if (result_json.contains("error"))
    {
        status_code = result_json["error"] == "Username already exists" ? "409 Conflict" : "400 Bad Request";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}
Response AuthController::create_admin_setup(const Request& req)
{
    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string username, password;

    try
    {
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing username or password"})", "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_username(username))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_password(password))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    std::string password_hash = PasswordHash::hash_password(password);

    std::string result = user_repository.create_user(username, password_hash, "admin");
    json result_json = json::parse(result);

    std::string status_code = "201 Created";
    if (result_json.contains("error"))
    {
        status_code = result_json["error"] == "Username already exists" ? "409 Conflict" : "400 Bad Request";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}
Response AuthController::create_worker_setup(const Request& req)
{
    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string username, password;

    try
    {
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing username or password"})", "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_username(username))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_password(password))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    std::string password_hash = PasswordHash::hash_password(password);

    std::string result = user_repository.create_user(username, password_hash, "employee");
    json result_json = json::parse(result);

    std::string status_code = "201 Created";
    if (result_json.contains("error"))
    {
        status_code = result_json["error"] == "Username already exists" ? "409 Conflict" : "400 Bad Request";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}
Response AuthController::logout(const Request& req)
{
    auto auth = require_auth(req);
    if (!auth.ok) return auth.error;

    SessionStore::instance().remove(*bearer_token(req));

    return Response{ R"({"status":"logged out"})", "application/json" };
}

Response AuthController::logout_all(const Request& req)
{
    auto auth = require_auth(req);
    if (!auth.ok) return auth.error;

    size_t closed = SessionStore::instance().remove_all_for_user(auth.user_id);

    json response = { {"status", "logged out everywhere"}, {"sessions_closed", closed} };
    return Response{ response.dump(), "application/json" };
}
Response AuthController::change_password(const Request& req)
{
    auto auth = require_auth(req);
    if (!auth.ok) return auth.error;

    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string current_password, new_password;

    try
    {
        current_password = body.at("current_password").get<std::string>();
        new_password = body.at("new_password").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing current_password or new_password"})", "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_password(new_password))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    auto user = user_repository.find_by_id(auth.user_id);

    if (!user)
    {
        return Response{ R"({"error":"User not found"})", "application/json", "404 Not Found" };
    }

    if (!PasswordHash::verify_password(current_password, user->password_hash))
    {
        return Response{ R"({"error":"Current password is incorrect"})", "application/json", "401 Unauthorized" };
    }

    if (current_password == new_password)
    {
        return Response{ R"({"error":"New password must differ from current password"})", "application/json", "400 Bad Request" };
    }

    std::string new_hash = PasswordHash::hash_password(new_password);
    std::string result = user_repository.update_password(auth.user_id, new_hash);
    json result_json = json::parse(result);

    if (result_json.contains("error"))
    {
        return Response{ result_json.dump(), "application/json", "500 Internal Server Error" };
    }

    // ќтзываем все токены Ч придЄтс€ войти заново с новым паролем
    SessionStore::instance().remove_all_for_user(auth.user_id);

    return Response{ R"({"status":"password updated, please log in again"})", "application/json" };
}