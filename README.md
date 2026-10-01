# Тесты API (PowerShell + curl.exe)

Запускайте сервер и выполняйте блоки по порядку. `-k` нужен из-за самоподписанного сертификата.
Значения с пробелами передавайте через файл (`--data-binary "@file.json"`), иначе PowerShell ломает кавычки и обрезает тело на первом пробеле.

Везде, где написано `$tokenX` — это реальная переменная PowerShell, не текстовый плейсхолдер. Подставлять вручную строку вида `АДМИН_ТОКЕН` нельзя, сервер ответит `401 Invalid or expired token`.

## 0. Подготовка

```sql
SELECT id, product_name, price, stock_quantity FROM Products;
SELECT id, login, role FROM Users;
```

Если `Products` пустая:
```sql
INSERT INTO Products (type, product_name, cost, price, stock_quantity) VALUES
    ('Engine', 'V6 Engine', 45000, 60000, 5),
    ('Brake System', 'Brake Pads', 800, 1500, 20),
    ('Suspension', 'Shock Absorber', 2500, 4000, 12);
```

## 1. Первый администратор

```powershell
curl.exe -k -X POST "https://localhost:8080/setup/create-admin" -H "Content-Type: application/json" -d '{\"username\":\"admin\",\"password\":\"admin123\"}'
```
`201` или `409`, если уже есть.

```sql
UPDATE Users SET role = 'admin' WHERE login = 'admin';
```
**После использования удалите маршрут `/setup/create-admin` из кода.**

## 2. Регистрация

```powershell
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"buyer1\",\"password\":\"buyer12345\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"buyer2\",\"password\":\"buyer22345\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"buyer1\",\"password\":\"buyer12345\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"ab\",\"password\":\"password123\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"newuser1\",\"password\":\"123\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{\"username\":\"sneaky\",\"password\":\"password123\",\"role\":\"admin\"}'
curl.exe -k -X POST "https://localhost:8080/register" -H "Content-Type: application/json" -d '{username:invalid}'
```
Ожидается: `201`; `201`; `409 Username already exists`; `400` (короткий логин); `400` (короткий пароль); `201`, но роль `sneaky` в БД — `customer`, не `admin`; `400 Invalid JSON`.

```sql
SELECT login, role FROM Users;
```

## 3. Логин, /me, logout, смена пароля

```powershell
$r = Invoke-RestMethod -Uri "https://localhost:8080/login" -Method POST -ContentType "application/json" -Body '{"username":"admin","password":"admin123"}' -SkipCertificateCheck
$tokenAdmin = $r.token
$r.user_id

$r = Invoke-RestMethod -Uri "https://localhost:8080/login" -Method POST -ContentType "application/json" -Body '{"username":"buyer1","password":"buyer12345"}' -SkipCertificateCheck
$tokenBuyer1 = $r.token
$userIdBuyer1 = $r.user_id

$r = Invoke-RestMethod -Uri "https://localhost:8080/login" -Method POST -ContentType "application/json" -Body '{"username":"buyer2","password":"buyer22345"}' -SkipCertificateCheck
$tokenBuyer2 = $r.token
```
Проверьте, что в каждой `$token...` реально лежит длинная строка (`$tokenAdmin` и т.д.), а не пусто.

```powershell
curl.exe -k -X POST "https://localhost:8080/login" -H "Content-Type: application/json" -d '{\"username\":\"admin\",\"password\":\"wrongpass\"}'
```
`401 Invalid credentials`.

```powershell
curl.exe -k -X GET "https://localhost:8080/me" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X GET "https://localhost:8080/me"
```
`200` с `role: customer`; `401`.

```powershell
curl.exe -k -X POST "https://localhost:8080/logout" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X GET "https://localhost:8080/me" -H "Authorization: Bearer $tokenBuyer1"
```
`200 logged out`; `401`. После этого войдите заново под buyer1, чтобы получить свежий `$tokenBuyer1` для дальнейших тестов.

Смена пароля:
```powershell
curl.exe -k -X PUT "https://localhost:8080/me/password" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"current_password\":\"buyer12345\",\"new_password\":\"newpass123\"}'
curl.exe -k -X GET "https://localhost:8080/me" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X POST "https://localhost:8080/login" -H "Content-Type: application/json" -d '{\"username\":\"buyer1\",\"password\":\"buyer12345\"}'
curl.exe -k -X POST "https://localhost:8080/login" -H "Content-Type: application/json" -d '{\"username\":\"buyer1\",\"password\":\"newpass123\"}'
```
Ожидается: `200 password updated, please log in again`; старый токен — `401`; логин со старым паролем — `401`; логин с новым — `200`.

Сохраните новый токен buyer1:
```powershell
$r = Invoke-RestMethod -Uri "https://localhost:8080/login" -Method POST -ContentType "application/json" -Body '{"username":"buyer1","password":"newpass123"}' -SkipCertificateCheck
$tokenBuyer1 = $r.token
```

Ошибки смены пароля:
```powershell
curl.exe -k -X PUT "https://localhost:8080/me/password" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"current_password\":\"wrongpass\",\"new_password\":\"anything123\"}'
curl.exe -k -X PUT "https://localhost:8080/me/password" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"current_password\":\"newpass123\",\"new_password\":\"newpass123\"}'
curl.exe -k -X PUT "https://localhost:8080/me/password" -H "Content-Type: application/json" -d '{\"current_password\":\"a\",\"new_password\":\"b\"}'
```
`401` (неверный текущий); `400` (новый совпадает со старым); `401` (нет токена).

## 4. Товары — чтение

```powershell
curl.exe -k -X GET "https://localhost:8080/products"
curl.exe -k -X GET "https://localhost:8080/products/1"
curl.exe -k -X GET "https://localhost:8080/products/99999"
curl.exe -k -X GET "https://localhost:8080/products/type/Engine"
curl.exe -k -X GET "https://localhost:8080/products/type/Brake%20System"
```
Список без `cost`; товар; `404`; товары категории; категория с пробелом.

## 5. Товары — запись и права

```powershell
'{"type":"Electrical","name":"Car Battery","cost":3000,"price":4500,"stock_quantity":10}' | Set-Content -NoNewline -Encoding utf8 product.json
curl.exe -k -X POST "https://localhost:8080/products" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" --data-binary "@product.json"

curl.exe -k -X PUT "https://localhost:8080/products/1" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"price\":62000,\"stock_quantity\":5}'
curl.exe -k -X PUT "https://localhost:8080/products/99999" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"price\":100,\"stock_quantity\":1}'
curl.exe -k -X DELETE "https://localhost:8080/products/99999" -H "Authorization: Bearer $tokenAdmin"
```
`201`; `200`; `404`; `404`.

```powershell
curl.exe -k -X POST "https://localhost:8080/products" -H "Content-Type: application/json" -d '{\"type\":\"Engine\",\"name\":\"NoAuth\",\"cost\":100,\"price\":500,\"stock_quantity\":5}'
curl.exe -k -X POST "https://localhost:8080/products" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"type\":\"Engine\",\"name\":\"CustomerTry\",\"cost\":100,\"price\":500,\"stock_quantity\":5}'
curl.exe -k -X DELETE "https://localhost:8080/products/1" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X POST "https://localhost:8080/products" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"type\":\"Engine\",\"name\":\"BadPrice\",\"cost\":100,\"price\":-500,\"stock_quantity\":5}'
```
`401`; `403` (покупатель не может добавлять товары); `403` (и не может удалять); `400` (цена).

## 6. Каталог — поиск, пагинация, сортировка

```powershell
curl.exe -k -X GET "https://localhost:8080/products/search?search=Brake&limit=5&offset=0"
curl.exe -k -X GET "https://localhost:8080/products/search?type=Engine&sort_by=price&sort_dir=DESC"
curl.exe -k -X GET "https://localhost:8080/products/search?limit=9999"
```
Ответ с `items`, `total`, `limit`, `offset`; сортировка по убыванию цены; `limit` обрезан до 100, даже если запрошено больше.

## 7. Корзина (привязана к токену)

```powershell
curl.exe -k -X GET "https://localhost:8080/cart"
```
`401`.

```powershell
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":1,\"quantity\":2}'
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":2,\"quantity\":1}'
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":1,\"quantity\":3}'
curl.exe -k -X GET "https://localhost:8080/cart" -H "Authorization: Bearer $tokenBuyer1"
```
`201` ×3; у product_id=1 количество суммировалось до 5.

```powershell
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":1,\"quantity\":0}'
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":99999,\"quantity\":1}'
```
`400` (нулевое количество); `404 Product not found` (товара не существует).

```powershell
curl.exe -k -X GET "https://localhost:8080/cart/preview" -H "Authorization: Bearer $tokenBuyer1"
```
Позиции с актуальными ценами, `has_issues: false`.

```powershell
curl.exe -k -X PUT "https://localhost:8080/cart/update" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":1,\"quantity\":10}'
curl.exe -k -X PUT "https://localhost:8080/cart/update" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{\"product_id\":9999,\"quantity\":5}'
curl.exe -k -X DELETE "https://localhost:8080/cart/remove/2" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X DELETE "https://localhost:8080/cart/remove/2" -H "Authorization: Bearer $tokenBuyer1"
```
`200`; `404 Item not found in cart`; `200`; `404`.

## 8. Изоляция корзин между пользователями

```powershell
curl.exe -k -X POST "https://localhost:8080/cart/add" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer2" -d '{\"product_id\":3,\"quantity\":1}'
curl.exe -k -X GET "https://localhost:8080/cart" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X GET "https://localhost:8080/cart" -H "Authorization: Bearer $tokenBuyer2"
```
Корзина buyer1 не содержит товар 3, корзина buyer2 содержит только его.

## 9. Checkout — данные доставки

```powershell
'{"delivery":{"recipient_name":"","phone":"123","city":"Odesa","address":"x"}}' | Set-Content -NoNewline -Encoding utf8 bad_checkout.json
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" --data-binary "@bad_checkout.json"
```
`400` (пустое имя получателя, короткий телефон).

```powershell
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" -d '{}'
```
`400 Missing delivery information`.

```powershell
'{"delivery":{"recipient_name":"Ivan Petrov","phone":"+380991234567","city":"Odesa","address":"vul. Pushkinska 10"}}' | Set-Content -NoNewline -Encoding utf8 checkout.json
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" --data-binary "@checkout.json"
```
`201` с `order_id`, `payment_ref`, `total`, `data`, `signature`. Запишите `order_id` → далее `$orderA`.

```powershell
curl.exe -k -X GET "https://localhost:8080/cart" -H "Authorization: Bearer $tokenBuyer1"
```
Корзина пуста после успешного checkout.

```powershell
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer1" --data-binary "@checkout.json"
```
`400 Cart is empty` (повторный checkout без новых товаров в корзине).

## 10. Checkout с явными items и нехваткой остатка

```powershell
'{"delivery":{"recipient_name":"Petro Buyer","phone":"+380501112233","city":"Kyiv","address":"vul. Khreshchatyk 1"},"items":[{"product_id":2,"quantity":999}]}' | Set-Content -NoNewline -Encoding utf8 c_insufficient.json
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer2" --data-binary "@c_insufficient.json"
```
`{"error":"Insufficient stock","product_id":2,"available":..}`.

```sql
SELECT id, stock_quantity FROM Products WHERE id = 2;
```
Остаток не изменился после неудачной попытки.

## 11. Заказы — доступ по владельцу

```powershell
curl.exe -k -X GET "https://localhost:8080/orders/$orderA" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X GET "https://localhost:8080/orders/$orderA" -H "Authorization: Bearer $tokenBuyer2"
curl.exe -k -X GET "https://localhost:8080/orders/$orderA" -H "Authorization: Bearer $tokenAdmin"
curl.exe -k -X GET "https://localhost:8080/orders/$orderA"
```
`200` с блоком `delivery` для владельца; `404` для чужого (не `403`); `200` для персонала; `401` без токена.

```powershell
curl.exe -k -X GET "https://localhost:8080/me/orders" -H "Authorization: Bearer $tokenBuyer1"
```
Список заказов buyer1, включая `$orderA`.

## 12. Отмена заказа покупателем

```powershell
curl.exe -k -X PUT "https://localhost:8080/orders/$orderA/cancel" -H "Authorization: Bearer $tokenBuyer1"
```
`200 order cancelled`.

```sql
SELECT status FROM Orders WHERE order_id = <order_a_id>;        -- Cancelled
SELECT status FROM Payments WHERE order_id = <order_a_id>;      -- Failed
SELECT id, stock_quantity FROM Products WHERE id = 1;           -- выросло на купленное количество
```

```powershell
curl.exe -k -X PUT "https://localhost:8080/orders/$orderA/cancel" -H "Authorization: Bearer $tokenBuyer1"
```
`409 Order cannot be cancelled` (уже отменён).

```powershell
curl.exe -k -X PUT "https://localhost:8080/orders/1/cancel" -H "Authorization: Bearer $tokenBuyer2"
```
`404 Order not found` — чужой заказ, buyer2 не должен даже узнать, что заказ №1 существует.

```powershell
curl.exe -k -X PUT "https://localhost:8080/orders/1/cancel"
```
`401` без токена.

## 13. Админ — все заказы и смена статуса с проверкой переходов

Создайте новый заказ под buyer2 для этого блока (повторите checkout из раздела 9 с токеном buyer2), запишите `order_id` → `$orderB`.

```powershell
curl.exe -k -X GET "https://localhost:8080/admin/orders" -H "Authorization: Bearer $tokenAdmin"
curl.exe -k -X GET "https://localhost:8080/admin/orders" -H "Authorization: Bearer $tokenBuyer1"
curl.exe -k -X GET "https://localhost:8080/admin/orders"
```
`200` со всеми заказами; `403` (покупателю нельзя); `401`.

Проверка цепочки переходов на `$orderB`:
```powershell
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Completed\"}'
```
`409 Invalid status transition` — из `New` сразу в `Completed` нельзя.

```powershell
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Processing\"}'
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Shipped\"}'
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Cancelled\"}'
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Completed\"}'
```
`200` (New→Processing); `200` (Processing→Shipped); `409` (Shipped→Cancelled запрещён); `200` (Shipped→Completed).

```powershell
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"New\"}'
curl.exe -k -X PUT "https://localhost:8080/admin/orders/99999/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Shipped\"}'
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderB/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Teleported\"}'
```
`409` (Completed — конечный статус); `404` (заказа нет); `400` (невалидное значение статуса).

## 14. Отмена через админ-маршрут возвращает склад

```sql
SELECT id, stock_quantity FROM Products WHERE id = 1;  -- запомните значение
```
Создайте ещё один заказ (checkout под любым покупателем с product_id=1), запишите `$orderC`.

```powershell
curl.exe -k -X PUT "https://localhost:8080/admin/orders/$orderC/status" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenAdmin" -d '{\"status\":\"Cancelled\"}'
```
```sql
SELECT id, stock_quantity FROM Products WHERE id = 1;  -- выросло на купленное количество
```

## 15. Автоотмена по таймауту

```powershell
curl.exe -k -X POST "https://localhost:8080/checkout" -H "Content-Type: application/json" -H "Authorization: Bearer $tokenBuyer2" --data-binary "@checkout.json"
```
Запишите `order_id` → `$orderD`.

```sql
UPDATE Orders SET created_at = DATEADD(MINUTE, -60, GETDATE()) WHERE order_id = <order_d_id>;
SELECT stock_quantity FROM Products WHERE id = <product_id>;  -- запомните
```
Подождите около минуты (интервал проверки `OrderCleaner`), затем:
```sql
SELECT status FROM Orders WHERE order_id = <order_d_id>;       -- Cancelled
SELECT stock_quantity FROM Products WHERE id = <product_id>;   -- выросло
```
В консоли сервера должна появиться строка `Auto-cancelled unpaid orders: N`.

## 16. Платёжный callback

```powershell
curl.exe -k -X POST "https://localhost:8080/payment/callback" -H "Content-Type: application/x-www-form-urlencoded" -d "data=fake&signature=fake"
curl.exe -k -X POST "https://localhost:8080/payment/callback" -H "Content-Type: application/x-www-form-urlencoded" -d "nothing=here"
```
`403` (подпись не сходится); `400`.

Полный цикл с реальной подписью — только через sandbox LiqPay после настройки домена.

## 17. CORS

```powershell
curl.exe -k -i -X GET "https://localhost:8080/products"
curl.exe -k -i -X OPTIONS "https://localhost:8080/cart/add"
```
В заголовках первого ответа — `Access-Control-Allow-Origin: *`. Второй запрос — `204 No Content` с теми же CORS-заголовками.

Проверка из браузера: откройте любую страницу не на `localhost:8080` (не `file://`) и в консоли (F12):
```javascript
fetch("https://localhost:8080/products").then(r => r.json()).then(console.log)
```
Не должно быть ошибки `blocked by CORS policy`.

## 18. Нагрузка

```powershell
1..15 | ForEach-Object -Parallel {
    curl.exe -k -X GET "https://localhost:8080/products"
} -ThrottleLimit 15
```
15 успешных ответов без ошибок соединения.

## 19. Финальная проверка в SSMS

```sql
SELECT * FROM Users;
SELECT * FROM Orders ORDER BY created_at DESC;
SELECT * FROM OrderItems ORDER BY order_id DESC;
SELECT * FROM Payments ORDER BY created_at DESC;
SELECT * FROM CartItems;
SELECT id, product_name, stock_quantity FROM Products;
```

## Чек-лист регрессии

- Недостающий объект (`UPDATE`/`DELETE` без совпадений) даёт `404`, не `500`. При `500` в консоли обязательно должна быть строка `ODBC Error`.
- Чужой заказ или чужая попытка отмены — `404`, никогда не `403` (не подтверждаем факт существования чужих данных).
- `cost` нигде не попадает в ответы `/products`.
- Переходы статусов заказа идут только по цепочке `New → Processing → Shipped → Completed`, отмена — только из `New`/`Processing`.
- Токен после `/logout` или смены пароля больше не работает ни в одном маршруте.
- Одноразовые маршруты (`/setup/create-admin`) удалены перед выкладкой.
