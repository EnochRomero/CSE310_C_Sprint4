#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

class Transaction {
public:
    Transaction(int id, std::string description, std::string category,
                double amount, bool isIncome)
        : id(id), description(std::move(description)), category(std::move(category)),
          amount(amount), isIncome(isIncome) {}

    int id;
    std::string description;
    std::string category;
    double amount;
    bool isIncome;
};

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string escapeHtml(const std::string& value) {
    std::string escaped;
    for (char character : value) {
        switch (character) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += character;
        }
    }
    return escaped;
}

std::string replaceAll(std::string value, const std::string& token,
                       const std::string& replacement) {
    std::size_t position = 0;
    while ((position = value.find(token, position)) != std::string::npos) {
        value.replace(position, token.length(), replacement);
        position += replacement.length();
    }
    return value;
}

std::string formatMoney(double amount) {
    std::ostringstream output;
    output << "$" << std::fixed << std::setprecision(2) << amount;
    return output.str();
}

std::string renderTransactions(const std::vector<Transaction>& transactions) {
    if (transactions.empty()) {
        return "<li class=\"empty-state\"><span class=\"empty-art\">*</span>"
               "<h3>It starts with one.</h3>"
               "<p>Add your first transaction and it will show up here.</p></li>";
    }

    std::ostringstream rows;
    for (auto item = transactions.rbegin(); item != transactions.rend(); ++item) {
        const std::string kind = item->isIncome ? "income" : "expense";
        const std::string sign = item->isIncome ? "+" : "-";
        rows << "<li class=\"transaction-item\">"
             << "<span class=\"transaction-icon " << kind << "\" aria-hidden=\"true\">"
             << (item->isIncome ? "&darr;" : "&uarr;") << "</span>"
             << "<div class=\"transaction-details\"><p class=\"transaction-title\">"
             << escapeHtml(item->description) << "</p><p class=\"transaction-meta\">"
             << escapeHtml(item->category) << "</p></div>"
             << "<div class=\"transaction-right\"><span class=\"transaction-amount "
             << kind << "\">" << sign << formatMoney(item->amount) << "</span>"
             << "<form class=\"remove-form\" method=\"post\" action=\"/delete\">"
             << "<input type=\"hidden\" name=\"id\" value=\"" << item->id << "\">"
             << "<button class=\"remove-button\" type=\"submit\">Remove</button>"
             << "</form></div></li>";
    }
    return rows.str();
}

std::string renderPage(const std::vector<Transaction>& transactions) {
    std::string page = readFile("index.html");
    double income = 0.0;
    double expenses = 0.0;
    for (const Transaction& transaction : transactions) {
        if (transaction.isIncome) income += transaction.amount;
        else expenses += transaction.amount;
    }

    page = replaceAll(page, "{{BALANCE}}", formatMoney(income - expenses));
    page = replaceAll(page, "{{INCOME}}", formatMoney(income));
    page = replaceAll(page, "{{EXPENSES}}", formatMoney(expenses));
    page = replaceAll(page, "{{COUNT}}", std::to_string(transactions.size()));
    page = replaceAll(page, "{{TRANSACTIONS}}", renderTransactions(transactions));
    return page;
}

std::string decodeFormValue(const std::string& value) {
    std::string decoded;
    for (std::size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '+') {
            decoded += ' ';
        } else if (value[i] == '%' && i + 2 < value.length()) {
            try {
                decoded += static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16));
                i += 2;
            } catch (...) {
                decoded += value[i];
            }
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

std::string formValue(const std::string& body, const std::string& key) {
    std::size_t start = 0;
    while (start < body.length()) {
        const std::size_t end = body.find('&', start);
        const std::string field = body.substr(start, end == std::string::npos ? end : end - start);
        const std::size_t equals = field.find('=');
        if (equals != std::string::npos && decodeFormValue(field.substr(0, equals)) == key) {
            return decodeFormValue(field.substr(equals + 1));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

bool receiveRequest(SOCKET client, std::string& request) {
    char buffer[4096];
    std::size_t expectedSize = 0;
    bool hasContentLength = false;

    while (request.size() < 16384) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return false;
        request.append(buffer, received);

        const std::size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd == std::string::npos) continue;
        if (!hasContentLength) {
            const std::string header = request.substr(0, headerEnd);
            const std::string marker = "Content-Length:";
            const std::size_t lengthPosition = header.find(marker);
            if (lengthPosition != std::string::npos) {
                const std::size_t valueStart = lengthPosition + marker.length();
                expectedSize = static_cast<std::size_t>(std::stoul(header.substr(valueStart)));
            }
            hasContentLength = true;
        }
        if (request.size() >= headerEnd + 4 + expectedSize) return true;
    }
    return false;
}

void sendResponse(SOCKET client, int status, const std::string& statusText,
                  const std::string& contentType, const std::string& body,
                  const std::string& extraHeaders = "") {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << statusText << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n" << extraHeaders << "\r\n" << body;
    const std::string message = response.str();
    std::size_t sent = 0;
    while (sent < message.size()) {
        const int count = send(client, message.data() + sent,
                               static_cast<int>(message.size() - sent), 0);
        if (count <= 0) break;
        sent += static_cast<std::size_t>(count);
    }
}

void redirectHome(SOCKET client) {
    sendResponse(client, 303, "See Other", "text/plain; charset=utf-8", "",
                 "Location: /\r\n");
}

void handleClient(SOCKET client, std::vector<Transaction>& transactions, int& nextId) {
    std::string request;
    if (!receiveRequest(client, request)) {
        sendResponse(client, 400, "Bad Request", "text/plain; charset=utf-8", "Bad request");
        return;
    }

    const std::size_t firstLineEnd = request.find("\r\n");
    if (firstLineEnd == std::string::npos) return;
    std::istringstream requestLine(request.substr(0, firstLineEnd));
    std::string method, path, version;
    requestLine >> method >> path >> version;
    const std::size_t headerEnd = request.find("\r\n\r\n");
    const std::string body = headerEnd == std::string::npos ? "" : request.substr(headerEnd + 4);

    if (method == "GET" && path == "/") {
        const std::string page = renderPage(transactions);
        if (page.empty()) {
            sendResponse(client, 500, "Server Error", "text/plain; charset=utf-8",
                         "Could not read index.html. Start the server from the project folder.");
        } else {
            sendResponse(client, 200, "OK", "text/html; charset=utf-8", page);
        }
    } else if (method == "GET" && path == "/styles.css") {
        const std::string css = readFile("styles.css");
        if (css.empty()) sendResponse(client, 404, "Not Found", "text/plain; charset=utf-8", "CSS file not found");
        else sendResponse(client, 200, "OK", "text/css; charset=utf-8", css);
    } else if (method == "POST" && path == "/add") {
        const std::string description = formValue(body, "description");
        const std::string category = formValue(body, "category");
        const std::string type = formValue(body, "type");
        const std::string amountText = formValue(body, "amount");
        try {
            std::size_t parsed = 0;
            const double amount = std::stod(amountText, &parsed);
            const bool isIncome = type == "income";
            if (!description.empty() && parsed == amountText.size() && std::isfinite(amount) && amount > 0 &&
                (type == "income" || type == "expense")) {
                transactions.emplace_back(nextId++, description.substr(0, 60),
                                          category.substr(0, 40), amount, isIncome);
            }
        } catch (...) {
            // Invalid form values are ignored; the browser returns to the tracker.
        }
        redirectHome(client);
    } else if (method == "POST" && path == "/delete") {
        try {
            const int id = std::stoi(formValue(body, "id"));
            transactions.erase(std::remove_if(transactions.begin(), transactions.end(),
                [id](const Transaction& transaction) { return transaction.id == id; }), transactions.end());
        } catch (...) {
            // Ignore a missing or invalid transaction id.
        }
        redirectHome(client);
    } else {
        sendResponse(client, 404, "Not Found", "text/plain; charset=utf-8", "Not found");
    }
}

int main() {
    WSADATA winsockData;
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        std::cerr << "Could not start Winsock.\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Could not create the server socket.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Could not listen on http://127.0.0.1:8080. The port may already be in use.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "Budget tracker running at http://127.0.0.1:8080\n"
              << "Keep this window open while using the tracker. Press Ctrl+C to stop.\n";
    std::vector<Transaction> transactions;
    int nextId = 1;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        handleClient(client, transactions, nextId);
        shutdown(client, SD_SEND);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
