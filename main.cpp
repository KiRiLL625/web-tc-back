#include <iostream>
#include <string>
#include <curl/curl.h> //для отправки http-запросов
#include <json/json.h> //для парсинга json-ответа 2gis api
#include <sstream>
#include <codecvt> //для настройки кодировки (кириллицу криво выводило)
#include <locale> //как и то, что сверху
#include <unordered_map> //для хранения категорий и названий магазинов в категории
#include <vector>
#include <crow.h> //отправка данных по "маршруту"

//функция для записи ответа в строку (для curl), по сути эта функция просто добавляет к строке userp содержимое contents
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

//функция для декодирования юникода (кириллицы) в utf-8, так как библиотека jsoncpp не умеет это делать
//но вообще я особо не разбирался, скорее всего, есть встроенная функция
//и можно эту функцию убрать
std::string decodeUnicode(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wideStr;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && str[i + 1] == 'u') {
            std::string hex = str.substr(i + 2, 4);
            int code;
            std::istringstream(hex) >> std::hex >> code;
            wideStr += static_cast<wchar_t>(code);
            i += 5;
        } else {
            wideStr += str[i];
        }
    }
    return converter.to_bytes(wideStr);
}

//функция для заполнения карты категорий и магазинов
//в качестве ключа используется категория, в качестве значения - вектор названий магазинов
//jsonData - json-ответ от 2gis api
//categoryMap - мапа из категорий и магазинов
void fillCategoryMap(const Json::Value& jsonData, std::unordered_map<std::string, std::vector<std::string>>& categoryMap) {
    const Json::Value& items = jsonData["result"]["items"]; //получаем массив магазинов
    for (const auto& item : items) { //проходим по каждому магазину
        std::string name = item["name"].asString(); //записываем в name название магазина
        for (const auto& rubric : item["rubrics"]) { //проходим по каждой категории магазина (их там несколько, но всего одна категория - primary)
            if (rubric["kind"].asString() == "primary") { //если категория - primary
                std::string category = rubric["name"].asString(); //записываем в category название категории
                categoryMap[category].push_back(name); //добавляем магазин в вектор магазинов категории
            }
        }
    }
}

//функция для вывода мапы категорий и магазинов
void printCategoryMap(const std::unordered_map<std::string, std::vector<std::string>>& categoryMap) {
    for (const auto& pair : categoryMap) { //проходим по каждой паре категория-магазины
        std::cout << "Category: " << pair.first << std::endl; //выводим категорию
        for (const auto& name : pair.second) { //проходим по каждому магазину в категории
            std::cout << "  - " << name << std::endl; //выводим магазин
        }
    }
}

int main() {
    CURL* curl; //указатель на структуру curl (для отправки http-запросов)
    CURLcode res; //код ответа curl (для проверки на ошибки)
    std::string readBuffer; //строка для записи ответа от 2gis api
    std::unordered_map<std::string, std::vector<std::string>> categoryMap; //мапа для хранения категорий и магазинов
    int total = 0; //количество магазинов в торгово-развлекательном центре (это вроде вообще не используется, так как на демо-ключ ограничения в 5 страниц ответа на запрос)

    curl_global_init(CURL_GLOBAL_DEFAULT); //инициализация curl
    curl = curl_easy_init(); //инициализация указателя на структуру curl

    if(curl) { //если указатель на структуру curl не нулевой
        std::string query = "Москва, Хорошо!, торгово-развлекательный центр"; //запрос для поиска тц
        char* encoded_query = curl_easy_escape(curl, query.c_str(), query.length()); //кодируем запрос
        if(encoded_query) { //если запрос закодирован
            //формируем url для поиска тц (items - поиск объектов, q - запрос, type - тип объекта (выбираем building, чтобы нам показало id здания), key - апи-ключ)
            std::string url = "https://catalog.api.2gis.com/3.0/items?q=" + std::string(encoded_query) + "&type=building&key=API_KEY";
            curl_free(encoded_query); //освобождаем память, выделенную для закодированного запроса

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); //устанавливаем конкретный url для запроса
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); //устанавливаем функцию для записи ответа в строку (ту самую в самом верху)
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer); //устанавливаем строку, в которую будет записываться ответ (в readBuffer)
            res = curl_easy_perform(curl); //выполняем запрос

            if(res != CURLE_OK) { //если запрос не выполнен успешно
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res)); //выводим ошибку в stderr
            } else {
                Json::Value jsonData; //создаем объект json-данных
                Json::Reader jsonReader; //создаем объект json-читателя
                if(jsonReader.parse(readBuffer, jsonData)){ //если ответ успешно распаршен в json-данные json-читателем
                    std::string formattedJson = jsonData.toStyledString(); //преобразуем json-данные в строку (не в 1 строку, а в несколько, чтобы было удобнее читать)
                    std::string decodedJson = decodeUnicode(formattedJson); //декодируем юникод (кириллицу) в utf-8
                    std::cout << decodedJson << std::endl; //выводим ответ
                }
            }

            readBuffer.clear(); //очищаем строку для ответа (без очищения в этой строке останется старый ответ и новый записан не будет)

            std::cout << std::endl; //пустая строка для красоты
            std::cout << std::endl;

            //этот запрос для получения списка магазинов внутри тц по id здания, который мы нашли в предыдущем запросе
            url = "https://catalog.api.2gis.com/3.0/items?building_id=4504235301257840&page=1&page_size=1&key=API_KEY";
            //тут дальше всё один в один вроде
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                Json::Value jsonData;
                Json::Reader jsonReader;
                if (jsonReader.parse(readBuffer, jsonData)) {
                    std::string formattedJson = jsonData.toStyledString();
                    std::string decodedJson = decodeUnicode(formattedJson);
                    std::cout << decodedJson << std::endl;

                    total = jsonData["result"]["total"].asInt(); //вроде не используется
                }
            }

            readBuffer.clear(); //очищаем буфер

            for(int i = 1; i <= 5; i++) { //вместо 5 должно быть total, ограничение накладывается для демо-ключа
                readBuffer.clear(); //очищаем буфер
                //запрос для получения рубрики (категории), к которой принадлежит каждое заведение внутри тц
                url = "https://catalog.api.2gis.com/3.0/items?building_id=4504235301257840&fields=items.rubrics&page=" + std::to_string(i) + "&page_size=10&key=API_KEY"; //page_size возможно должно быть 1, чтобы не нужно было округлять total
                //дальше всё +- то же самое
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                res = curl_easy_perform(curl);

                if (res != CURLE_OK) {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                } else {
                    Json::Value jsonData;
                    Json::Reader jsonReader;
                    if (jsonReader.parse(readBuffer, jsonData)) {
                        std::string formattedJson = jsonData.toStyledString();
                        std::string decodedJson = decodeUnicode(formattedJson);
                        std::cout << decodedJson << std::endl;
                        fillCategoryMap(jsonData, categoryMap); //заполняем мапу категорий и магазинов
                    }
                }
            }

            curl_easy_cleanup(curl); //освобождаем память, выделенную для структуры curl
        }
    }

    curl_global_cleanup(); //освобождаем память, выделенную для curl

    printCategoryMap(categoryMap); //выводим мапу категорий и магазинов

    crow::SimpleApp app; //создаем объект приложения crow

    //создаём маршрут для получения списка категорий (маршрут это путь, по которому можно получить данные, типа http://localhost:8080/categories)
    CROW_ROUTE(app, "/categories")
    ([&categoryMap]() { //будем в лямбде использовать categoryMap, поэтому передаём его по ссылке
        crow::json::wvalue x; //создаём объект json-данных
        std::vector<std::string> categories; //создаём вектор категорий
        for (const auto& pair : categoryMap) { //проходим по каждой паре категория-магазины
            categories.push_back(pair.first); //добавляем категорию в вектор категорий
        }
        x["categories"] = categories; //добавляем вектор категорий в json-данные
        auto res = crow::response(x); //создаём ответ с json-данными
        res.set_header("Content-Type", "application/json"); //устанавливаем заголовок Content-Type (это по дефолту надо, чтобы браузер понимал, что это json)
        res.set_header("Access-Control-Allow-Origin", "*"); //устанавливаем заголовок Access-Control-Allow-Origin (это для того, чтобы можно было делать запросы с любого домена, react-приложение работало на localhost:3000, а сервер на localhost:8080, поэтому надо было это добавить)
        return res; //возвращаем json-ответ
    });

    CROW_ROUTE(app, "/stores") //маршрут для получения списка магазинов в категории (http://localhost:8080/stores?category=Категория)
    ([&categoryMap](const crow::request& req) { //в параметрах уже есть req - это запрос, который пришёл на сервер (после category= нужно получить значение)
        auto category = req.url_params.get("category"); //получаем значение параметра category и запись в переменную category
        if (!category) { //если параметр category не передан
            return crow::response(400, "Category parameter is missing"); //возвращаем ответ с кодом 400
        }

        auto it = categoryMap.find(category); //ищем категорию в мапе категорий и магазинов
        if (it == categoryMap.end()) { //если категория не найдена
            return crow::response(404, "Category not found"); //возвращаем ответ с кодом 404
        }

        crow::json::wvalue x; //создаём объект json-данных
        x["stores"] = it->second; //добавляем вектор магазинов категории в json-данные
        auto res = crow::response(x); //создаём ответ с json-данными
        res.set_header("Content-Type", "application/json"); //устанавливаем параметры ответа
        res.set_header("Access-Control-Allow-Origin", "*");
        return res; //возвращаем json-ответ
    });

    app.port(8080).multithreaded().run(); //запускаем сервер на порту 8080 (http://localhost:8080)

    return 0;
}