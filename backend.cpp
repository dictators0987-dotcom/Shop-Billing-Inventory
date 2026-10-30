// Enables rand_s(), a CSPRNG from the CRT, for login salts, password-reset-proof
// session tokens, etc. Must be defined before the first <cstdlib> include.
#define _CRT_RAND_S

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "include/json.hpp"

using namespace std;
using json = nlohmann::ordered_json;

struct Product {
    string id;
    string name;
    double price = 0.0;
    double purchasePrice = 0.0;
    int stock = 0;
    string category = "General";
    string unit = "piece";
    int lowStockThreshold = 5;
};

// A credit ("khata") customer. Their running balance isn't stored here - it's computed
// on demand from sales.jsonl (sales linked to this id with amountPaid < total) minus
// customer_payments.jsonl (repayments recorded against this id).
struct Customer {
    string id;
    string name;
    string phone;
    string createdAt;
};

struct Response {
    int status = 200;
    string body;
    string contentType = "application/json; charset=utf-8";
};

vector<Product> products;
vector<Customer> customers;
string shopName = "MY SHOP NAME";
string shopPhone;
string shopAddress;
int nextProductNumber = 1;
int nextBillNumber = 1;
int nextCustomerNumber = 1;

// Login. authConfigured is false until the shop owner sets a username/password on
// first run; until then, /api/auth/setup is open and every other /api/* route is
// refused (there's nothing valid to check a session against yet).
string authUsername;
string authSaltHex;
string authHashHex;
bool authConfigured = false;
// Recovery question/answer, set at the same time as the password. recoveryConfigured is
// false for logins created before this feature existed (auth.txt simply has no lines for
// it) - those shops have no recovery path until they set one up, same as today.
string recoveryQuestion;
string recoveryAnswerSaltHex;
string recoveryAnswerHashHex;
bool recoveryConfigured = false;
map<string, time_t> sessions; // token -> expiry
const long long SESSION_DURATION_SECONDS = 12LL * 3600;
const size_t MAX_USERNAME_LENGTH = 60;
const size_t MIN_PASSWORD_LENGTH = 4;
const size_t MAX_PASSWORD_LENGTH = 200;
const size_t MAX_SECURITY_QUESTION_LENGTH = 200;
const size_t MIN_SECURITY_ANSWER_LENGTH = 2;
const size_t MAX_SECURITY_ANSWER_LENGTH = 200;
// Set when products.txt exists but cannot be parsed: writing would overwrite the
// shop's real data with an empty list, so product changes and sales are refused.
bool productsFileUnreadable = false;
int serverPort = 8080;

// Limits. Everything the browser sends is checked against these before it is stored.
const size_t MAX_REQUEST_SIZE = 256 * 1024;
const size_t MAX_HEADER_SIZE = 16 * 1024;
const int MAX_JSON_DEPTH = 16;
const int MAX_ACTIVE_CLIENTS = 512;
const int REQUEST_DEADLINE_MS = 10000;
const size_t MAX_BACKUPS_PER_FILE = 50;
const size_t MAX_NAME_LENGTH = 120;
const size_t MAX_LABEL_LENGTH = 60;
const size_t MAX_SHOP_FIELD_LENGTH = 200;
const size_t MAX_CUSTOMER_FIELD_LENGTH = 80;
const size_t MAX_PHONE_LENGTH = 30;
const size_t MAX_ID_LENGTH = 64;
const size_t MAX_BILL_ROWS = 500;
const double MAX_PRICE = 1000000;
const int MAX_STOCK = 1000000000;
const int MAX_QTY = 1000000;
const double MAX_PAYMENT = 1e12;
const double MAX_TAX_RATE = 100;

mutex dataMutex;
atomic<int> activeClients{0};

string getExecutableDirectory() {
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return ".";
    }

    string path(buffer);
    size_t pos = path.find_last_of("\\/");
    if (pos == string::npos) {
        return ".";
    }
    return path.substr(0, pos);
}

string joinPath(const string& directory, const string& fileName) {
    if (directory.empty()) return fileName;
    if (directory.back() == '\\' || directory.back() == '/') {
        return directory + fileName;
    }
    return directory + "\\" + fileName;
}

const string APP_DIRECTORY = getExecutableDirectory();
const string PRODUCTS_FILE = joinPath(APP_DIRECTORY, "products.txt");
const string SHOP_FILE = joinPath(APP_DIRECTORY, "shop_name.txt");
const string SALES_FILE = joinPath(APP_DIRECTORY, "sales.jsonl");
const string AUTH_FILE = joinPath(APP_DIRECTORY, "auth.txt");
const string CUSTOMERS_FILE = joinPath(APP_DIRECTORY, "customers.jsonl");
const string CUSTOMER_PAYMENTS_FILE = joinPath(APP_DIRECTORY, "customer_payments.jsonl");
const string BACKUP_DIRECTORY = joinPath(APP_DIRECTORY, "backups");

// The web page files the backend may serve. Only these exact paths are served, so
// data files, source code and anything outside the folder can never be requested.
const map<string, pair<string, string>> STATIC_FILES = {
    {"/", {"index.html", "text/html; charset=utf-8"}},
    {"/index.html", {"index.html", "text/html; charset=utf-8"}},
    {"/script.js", {"script.js", "text/javascript; charset=utf-8"}},
    {"/i18n.js", {"i18n.js", "text/javascript; charset=utf-8"}},
    {"/styles.css", {"styles.css", "text/css; charset=utf-8"}},
    {"/themes/counter.css", {"themes\\counter.css", "text/css; charset=utf-8"}},
    {"/themes/fandeck.css", {"themes\\fandeck.css", "text/css; charset=utf-8"}},
};

string currentTimestamp() {
    time_t now = time(nullptr);
    tm localTime{};
    tm* localTimePointer = localtime(&now);
    if (localTimePointer) {
        localTime = *localTimePointer;
    }
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

string backupTimestamp() {
    string value = currentTimestamp();
    replace(value.begin(), value.end(), ' ', '_');
    replace(value.begin(), value.end(), ':', '-');
    return value;
}

// Keeps only the newest MAX_BACKUPS_PER_FILE backups of one file. Backup names end in a
// sortable timestamp, so alphabetical order is age order.
void pruneBackups(const string& fileName) {
    vector<string> names;
    WIN32_FIND_DATAA found;
    string pattern = joinPath(BACKUP_DIRECTORY, fileName + ".*.bak");
    HANDLE handle = FindFirstFileA(pattern.c_str(), &found);
    if (handle == INVALID_HANDLE_VALUE) return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            names.push_back(found.cFileName);
        }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);

    if (names.size() <= MAX_BACKUPS_PER_FILE) return;
    sort(names.begin(), names.end());
    for (size_t i = 0; i + MAX_BACKUPS_PER_FILE < names.size(); ++i) {
        DeleteFileA(joinPath(BACKUP_DIRECTORY, names[i]).c_str());
    }
}

void backupFile(const string& filePath, const string& fileName) {
    if (GetFileAttributesA(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    CreateDirectoryA(BACKUP_DIRECTORY.c_str(), nullptr);
    string backupPath = joinPath(BACKUP_DIRECTORY, fileName + "." + backupTimestamp() + ".bak");
    CopyFileA(filePath.c_str(), backupPath.c_str(), FALSE);
    pruneBackups(fileName);
}

bool writeFileAtomically(const string& filePath, const string& content) {
    string temporaryPath = filePath + ".tmp";
    {
        ofstream out(temporaryPath, ios::binary | ios::trunc);
        if (!out) return false;
        out << content;
        out.flush();
        if (!out) return false;
    }

    // Sync tools such as OneDrive or antivirus scanners can briefly lock the
    // target file, so retry the replace a few times before giving up.
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (MoveFileExA(temporaryPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        Sleep(100);
    }
    cerr << "Unable to save " << filePath << " (error " << GetLastError() << ")" << endl;
    return false;
}

bool readFile(const string& filePath, string& content) {
    ifstream in(filePath, ios::binary);
    if (!in) return false;
    ostringstream buffer;
    buffer << in.rdbuf();
    content = buffer.str();
    return true;
}

string toLower(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return value;
}

// Prints numbers without scientific notation or 6-digit rounding
// (the ostream default turns 1234567 into 1.23457e+06).
string formatNumber(double value) {
    ostringstream oss;
    oss << setprecision(15) << value;
    return oss.str();
}

double roundMoney(double value) {
    return round(value * 100.0) / 100.0;
}

string trim(const string& value) {
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(start, end - start);
}

// Product and shop data is stored one field per line, so a field must never contain a
// line break or other control character.
string singleLine(const string& value) {
    string result = value;
    for (char& c : result) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = ' ';
    }
    return trim(result);
}

// ---------------------------------------------------------------- SHA-256 + randomness
//
// Minimal SHA-256 (public domain, adapted from Brad Conte's implementation) used only to
// store login passwords as a salted hash instead of plain text. Random bytes come from
// rand_s(), a CSPRNG the CRT exposes over RtlGenRandom - no extra library to link.

namespace sha256_impl {
    typedef unsigned char BYTE;
    typedef unsigned int WORD;

    struct Ctx {
        BYTE data[64];
        WORD dataLen = 0;
        unsigned long long bitLen = 0;
        WORD state[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
    };

    static const WORD K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    inline WORD rotr(WORD x, WORD n) { return (x >> n) | (x << (32 - n)); }

    void transform(Ctx& ctx, const BYTE block[64]) {
        WORD m[64];
        for (int i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<WORD>(block[j]) << 24) | (static_cast<WORD>(block[j + 1]) << 16)
                 | (static_cast<WORD>(block[j + 2]) << 8) | static_cast<WORD>(block[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            WORD s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
            WORD s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }

        WORD a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
        WORD e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];

        for (int i = 0; i < 64; ++i) {
            WORD S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            WORD ch = (e & f) ^ (~e & g);
            WORD temp1 = h + S1 + ch + K[i] + m[i];
            WORD S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            WORD maj = (a & b) ^ (a & c) ^ (b & c);
            WORD temp2 = S0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
        ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
    }

    void update(Ctx& ctx, const BYTE* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            ctx.data[ctx.dataLen++] = data[i];
            if (ctx.dataLen == 64) {
                transform(ctx, ctx.data);
                ctx.bitLen += 512;
                ctx.dataLen = 0;
            }
        }
    }

    void final(Ctx& ctx, BYTE hash[32]) {
        WORD i = ctx.dataLen;

        if (ctx.dataLen < 56) {
            ctx.data[i++] = 0x80;
            while (i < 56) ctx.data[i++] = 0x00;
        } else {
            ctx.data[i++] = 0x80;
            while (i < 64) ctx.data[i++] = 0x00;
            transform(ctx, ctx.data);
            memset(ctx.data, 0, 56);
        }

        ctx.bitLen += static_cast<unsigned long long>(ctx.dataLen) * 8;
        for (int j = 0; j < 8; ++j) {
            ctx.data[63 - j] = static_cast<BYTE>(ctx.bitLen >> (j * 8));
        }
        transform(ctx, ctx.data);

        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 8; ++k) {
                hash[k * 4 + j] = static_cast<BYTE>((ctx.state[k] >> (24 - j * 8)) & 0xff);
            }
        }
    }
}

string toHex(const unsigned char* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        result.push_back(digits[(data[i] >> 4) & 0xf]);
        result.push_back(digits[data[i] & 0xf]);
    }
    return result;
}

string sha256Hex(const string& input) {
    sha256_impl::Ctx ctx;
    sha256_impl::update(ctx, reinterpret_cast<const unsigned char*>(input.data()), input.size());
    unsigned char hash[32];
    sha256_impl::final(ctx, hash);
    return toHex(hash, 32);
}

// Cryptographically random hex string via rand_s() (RtlGenRandom), used for salts and
// session tokens - never anything predictable like rand() or a time seed.
string randomHex(size_t numBytes) {
    string bytes(numBytes, '\0');
    for (size_t i = 0; i < numBytes; ++i) {
        unsigned int value = 0;
        rand_s(&value);
        bytes[i] = static_cast<char>(value & 0xff);
    }
    return toHex(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
}

// Avoids leaking how many leading bytes of the hash matched via response timing.
bool constantTimeEquals(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

string dumpJson(const json& value) {
    return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

string errorJson(const string& message) {
    return dumpJson(json{{"error", message}});
}

Response jsonResponse(int status, const json& body) {
    return {status, dumpJson(body)};
}

Response errorResponse(int status, const string& message) {
    return {status, errorJson(message)};
}

string generateProductId() {
    string id;
    do {
        id = "product_" + to_string(nextProductNumber++);
    } while (any_of(products.begin(), products.end(), [&](const Product& p) { return p.id == id; }));
    return id;
}

Product* findProductById(const string& id) {
    for (auto& product : products) {
        if (product.id == id) {
            return &product;
        }
    }
    return nullptr;
}

// First line of products.txt in the current format. Lets loadProducts() know for certain
// that every line after it is 8-field records, instead of guessing the field width from
// the line count - see the comment on parseProducts() for why that guess was unsafe.
const string PRODUCTS_FORMAT_MARKER = "#products-v8";

bool saveProducts() {
    ostringstream content;
    content << PRODUCTS_FORMAT_MARKER << "\n";
    for (const auto& p : products) {
        content << p.id << "\n" << p.name << "\n" << formatNumber(p.price) << "\n" << p.stock << "\n"
            << p.category << "\n" << p.unit << "\n" << p.lowStockThreshold << "\n"
            << formatNumber(p.purchasePrice) << "\n";
    }
    backupFile(PRODUCTS_FILE, "products.txt");
    return writeFileAtomically(PRODUCTS_FILE, content.str());
}

bool parseDouble(const string& text, double& value) {
    try {
        size_t parsedLength = 0;
        value = stod(text, &parsedLength);
        return parsedLength == text.size() && isfinite(value);
    } catch (...) {
        return false;
    }
}

bool parseInt(const string& text, int& value) {
    try {
        size_t parsedLength = 0;
        value = stoi(text, &parsedLength);
        return parsedLength == text.size();
    } catch (...) {
        return false;
    }
}

// Parses product lines using a fixed number of lines per product.
// 8 = current format, 7 = before purchase price existed, 4 = original id/name/price/stock format.
// CAUTION: a line count that's a valid multiple of 8 is also a valid multiple of 4, so
// calling this with fieldsPerProduct 7 or 4 against current-format data (e.g. because the
// 8-field attempt failed due to one corrupted field) can "succeed" at misreading it as a
// different, bogus set of products. Only loadProducts() may use 7/4 as a one-time
// migration for files saved before PRODUCTS_FORMAT_MARKER existed - never as a fallback
// for a file already known to be in the current format.
bool parseProducts(const vector<string>& lines, size_t fieldsPerProduct, vector<Product>& result) {
    if (lines.empty() || lines.size() % fieldsPerProduct != 0) return false;

    vector<Product> loaded;
    for (size_t index = 0; index < lines.size(); index += fieldsPerProduct) {
        Product p;
        p.id = lines[index];
        p.name = lines[index + 1];
        if (!parseDouble(lines[index + 2], p.price)) return false;
        if (!parseInt(lines[index + 3], p.stock)) return false;
        if (fieldsPerProduct >= 7) {
            p.category = lines[index + 4];
            p.unit = lines[index + 5];
            if (!parseInt(lines[index + 6], p.lowStockThreshold)) return false;
        }
        if (fieldsPerProduct == 8 && !parseDouble(lines[index + 7], p.purchasePrice)) return false;
        loaded.push_back(p);
    }

    result = loaded;
    return true;
}

void loadProducts() {
    ifstream in(PRODUCTS_FILE);
    if (!in) {
        products.clear();
        saveProducts();
        return;
    }

    vector<string> lines;
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    in.close();

    if (lines.empty()) return;

    vector<Product> loaded;
    bool hasMarker = lines.front() == PRODUCTS_FORMAT_MARKER;
    bool parsed;
    if (hasMarker) {
        // The file says it's current-format: parse strictly as 8 fields per product. Never
        // fall back to 7/4 here - a line count divisible by 8 is also divisible by 4, so a
        // corrupted 8-field file can otherwise be silently misread as valid-but-wrong data
        // (see the warning on parseProducts()).
        vector<string> productLines(lines.begin() + 1, lines.end());
        parsed = parseProducts(productLines, 8, loaded);
    } else {
        // No marker: this file predates PRODUCTS_FORMAT_MARKER, so its field width is
        // genuinely ambiguous and has to be guessed once, newest format first.
        parsed = parseProducts(lines, 8, loaded) || parseProducts(lines, 7, loaded) || parseProducts(lines, 4, loaded);
    }

    if (!parsed) {
        productsFileUnreadable = true;
        cerr << "Warning: products.txt could not be read. It was left unchanged and product changes are disabled."
             << " Restore it from the backups folder and restart backend.exe." << endl;
        return;
    }
    products = loaded;
    if (!hasMarker) {
        // Upgrade the file to the marked format now, so this guess is never needed again.
        saveProducts();
    }

    // Continue numbering after the highest existing id so new products never reuse an id.
    for (const auto& p : products) {
        int number = 0;
        if (p.id.rfind("product_", 0) == 0 && parseInt(p.id.substr(8), number)) {
            nextProductNumber = max(nextProductNumber, number + 1);
        }
    }
}

string generateCustomerId() {
    string id;
    do {
        id = "customer_" + to_string(nextCustomerNumber++);
    } while (any_of(customers.begin(), customers.end(), [&](const Customer& c) { return c.id == id; }));
    return id;
}

Customer* findCustomerById(const string& id) {
    for (auto& customer : customers) {
        if (customer.id == id) return &customer;
    }
    return nullptr;
}

bool saveCustomers() {
    ostringstream content;
    for (const auto& cust : customers) {
        json record{{"id", cust.id}, {"name", cust.name}, {"phone", cust.phone}, {"createdAt", cust.createdAt}};
        content << dumpJson(record) << "\n";
    }
    backupFile(CUSTOMERS_FILE, "customers.jsonl");
    return writeFileAtomically(CUSTOMERS_FILE, content.str());
}

// customers.jsonl holds no sensitive data (unlike auth.txt), so a damaged line is simply
// skipped rather than refusing to start, same as sales.jsonl.
void loadCustomers() {
    ifstream in(CUSTOMERS_FILE, ios::binary);
    string line;
    vector<Customer> loaded;
    while (getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.front() != '{') continue;
        json record = json::parse(line, nullptr, false);
        if (record.is_discarded() || !record.is_object()) continue;
        if (!record.contains("id") || !record["id"].is_string()) continue;

        Customer cust;
        cust.id = record["id"].get<string>();
        cust.name = record.value("name", "");
        cust.phone = record.value("phone", "");
        cust.createdAt = record.value("createdAt", "");
        loaded.push_back(cust);
    }
    customers = loaded;

    for (const auto& cust : customers) {
        int number = 0;
        if (cust.id.rfind("customer_", 0) == 0 && parseInt(cust.id.substr(9), number)) {
            nextCustomerNumber = max(nextCustomerNumber, number + 1);
        }
    }
}

// Appends one repayment to the ledger. Like sales.jsonl, this file is append-only - a
// customer's balance is always recomputed from the full history, never stored directly,
// so there's nothing to get out of sync.
bool appendCustomerPayment(const json& payment) {
    string existingContent;
    if (readFile(CUSTOMER_PAYMENTS_FILE, existingContent)) {
        backupFile(CUSTOMER_PAYMENTS_FILE, "customer_payments.jsonl");
    }
    if (!existingContent.empty() && existingContent.back() != '\n') existingContent += "\n";
    existingContent += dumpJson(payment) + "\n";
    return writeFileAtomically(CUSTOMER_PAYMENTS_FILE, existingContent);
}

vector<json> loadCustomerPayments() {
    vector<json> payments;
    ifstream in(CUSTOMER_PAYMENTS_FILE, ios::binary);
    string line;
    while (getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.front() != '{') continue;
        json payment = json::parse(line, nullptr, false);
        if (!payment.is_discarded() && payment.is_object()) payments.push_back(move(payment));
    }
    return payments;
}

bool saveShopName() {
    ostringstream content;
    content << shopName << "\n" << shopPhone << "\n" << shopAddress;
    backupFile(SHOP_FILE, "shop_name.txt");
    return writeFileAtomically(SHOP_FILE, content.str());
}

void loadShopName() {
    ifstream in(SHOP_FILE);
    if (!in) {
        shopName = "MY SHOP NAME";
        saveShopName();
        return;
    }

    getline(in, shopName);
    getline(in, shopPhone);
    getline(in, shopAddress);
    shopName = singleLine(shopName);
    shopPhone = singleLine(shopPhone);
    shopAddress = singleLine(shopAddress);
    if (shopName.empty()) {
        shopName = "MY SHOP NAME";
    }
}

bool saveAuth() {
    ostringstream content;
    content << authUsername << "\n" << authSaltHex << "\n" << authHashHex << "\n"
             << recoveryQuestion << "\n" << recoveryAnswerSaltHex << "\n" << recoveryAnswerHashHex;
    backupFile(AUTH_FILE, "auth.txt");
    return writeFileAtomically(AUTH_FILE, content.str());
}

// No auth.txt means the shop owner has not set a username/password yet, so
// /api/auth/setup is still open (see handleAuthSetup). The three recovery lines are
// missing entirely in auth.txt files saved before that feature existed - getline simply
// leaves those strings empty, which is exactly "no recovery question configured".
void loadAuth() {
    ifstream in(AUTH_FILE);
    if (!in) {
        authConfigured = false;
        return;
    }

    string username, salt, hash, question, answerSalt, answerHash;
    getline(in, username);
    getline(in, salt);
    getline(in, hash);
    getline(in, question);
    getline(in, answerSalt);
    getline(in, answerHash);
    authUsername = singleLine(username);
    authSaltHex = singleLine(salt);
    authHashHex = singleLine(hash);
    recoveryQuestion = singleLine(question);
    recoveryAnswerSaltHex = singleLine(answerSalt);
    recoveryAnswerHashHex = singleLine(answerHash);
    authConfigured = !authUsername.empty() && !authSaltHex.empty() && !authHashHex.empty();
    recoveryConfigured = !recoveryQuestion.empty() && !recoveryAnswerSaltHex.empty() && !recoveryAnswerHashHex.empty();
}

// Reads every valid bill from sales.jsonl. Damaged lines are skipped so one bad
// record cannot break the whole history.
vector<json> loadSales() {
    vector<json> sales;
    ifstream in(SALES_FILE, ios::binary);
    string line;
    while (getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.front() != '{') continue;
        json sale = json::parse(line, nullptr, false);
        if (!sale.is_discarded() && sale.is_object()) {
            sales.push_back(move(sale));
        }
    }
    return sales;
}

void loadNextBillNumber() {
    int highest = 0;
    int count = 0;
    for (const auto& sale : loadSales()) {
        ++count;
        if (sale.contains("billNo") && sale["billNo"].is_number_integer()) {
            highest = max(highest, sale["billNo"].get<int>());
        }
    }
    nextBillNumber = max(highest, count) + 1;
}

bool saveSale(const string& saleJson) {
    string existingContent;
    if (readFile(SALES_FILE, existingContent)) {
        backupFile(SALES_FILE, "sales.jsonl");
    }

    if (!existingContent.empty() && existingContent.back() != '\n') {
        existingContent += "\n";
    }
    existingContent += saleJson + "\n";
    return writeFileAtomically(SALES_FILE, existingContent);
}

// Rewrites sales.jsonl entirely with the given records, in order. Used when an existing
// bill is edited (see handleAddItemsToSale) rather than appended to.
bool saveAllSales(const vector<json>& sales) {
    backupFile(SALES_FILE, "sales.jsonl");
    string content;
    for (const auto& sale : sales) {
        content += dumpJson(sale) + "\n";
    }
    return writeFileAtomically(SALES_FILE, content);
}

// ---------------------------------------------------------------- request body validation

// Rejects absurdly nested JSON before parsing it.
bool jsonDepthOk(const string& text) {
    int depth = 0;
    bool inString = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
        } else if (c == '{' || c == '[') {
            if (++depth > MAX_JSON_DEPTH) return false;
        } else if (c == '}' || c == ']') {
            --depth;
        }
    }
    return true;
}

bool parseBody(const string& bodyText, json& body, string& error) {
    if (!jsonDepthOk(bodyText)) {
        error = "Request body is nested too deeply.";
        return false;
    }
    body = json::parse(bodyText, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        error = "Request body must be a valid JSON object.";
        return false;
    }
    return true;
}

// Each reader returns false (and sets error) when the field has the wrong type or is out of range.
// A missing or null field takes the fallback value.
bool readText(const json& body, const string& key, size_t maxLength, const string& label, string& value, string& error) {
    if (!body.contains(key) || body[key].is_null()) return true;
    if (!body[key].is_string()) {
        error = label + " must be text.";
        return false;
    }
    string text = singleLine(body[key].get<string>());
    if (text.size() > maxLength) {
        error = label + " is too long (maximum " + to_string(maxLength) + " characters).";
        return false;
    }
    value = text;
    return true;
}

bool readNumber(const json& body, const string& key, double minimum, double maximum, const string& label, double& value, string& error) {
    if (!body.contains(key) || body[key].is_null()) return true;
    const json& field = body[key];
    if (!field.is_number() || !isfinite(field.get<double>()) || field.get<double>() < minimum || field.get<double>() > maximum) {
        error = "Invalid " + label + " value.";
        return false;
    }
    value = field.get<double>();
    return true;
}

bool readInteger(const json& body, const string& key, int minimum, int maximum, const string& label, int& value, string& error) {
    if (!body.contains(key) || body[key].is_null()) return true;
    const json& field = body[key];
    double number = field.is_number() ? field.get<double>() : NAN;
    if (!isfinite(number) || number != floor(number) || number < minimum || number > maximum) {
        error = "Invalid " + label + " value.";
        return false;
    }
    value = static_cast<int>(number);
    return true;
}

// ---------------------------------------------------------------- handlers

json productToJson(const Product& product) {
    return json{
        {"id", product.id},
        {"name", product.name},
        {"price", product.price},
        {"purchasePrice", product.purchasePrice},
        {"stock", product.stock},
        {"category", product.category},
        {"unit", product.unit},
        {"lowStockThreshold", product.lowStockThreshold},
    };
}

json shopToJson() {
    return json{{"shopName", shopName}, {"shopPhone", shopPhone}, {"shopAddress", shopAddress}};
}

Response unreadableProductsResponse() {
    return errorResponse(503, "products.txt could not be read, so changes are disabled to protect your data. Restore it from the backups folder and restart backend.exe.");
}

Response handleGetProducts() {
    json list = json::array();
    for (const auto& product : products) list.push_back(productToJson(product));
    return jsonResponse(200, list);
}

Response handleBackupNow() {
    backupFile(PRODUCTS_FILE, "products.txt");
    backupFile(SHOP_FILE, "shop_name.txt");
    backupFile(SALES_FILE, "sales.jsonl");
    return jsonResponse(200, json{{"message", "Backup created successfully."}});
}

// Reads and validates product fields shared by create and update. Returns an error message, or "" when valid.
string readProductFields(const string& bodyText, const Product& defaults, Product& result) {
    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return error;

    result = defaults;
    result.name = "";
    result.price = -1;
    result.stock = -1;
    if (!readText(body, "name", MAX_NAME_LENGTH, "Product name", result.name, error)) return error;
    if (!readText(body, "category", MAX_LABEL_LENGTH, "Category", result.category, error)) return error;
    if (!readText(body, "unit", MAX_LABEL_LENGTH, "Unit", result.unit, error)) return error;
    if (!readNumber(body, "price", 0, MAX_PRICE, "price", result.price, error)) return error;
    if (!readNumber(body, "purchasePrice", 0, MAX_PRICE, "purchase price", result.purchasePrice, error)) return error;
    if (!readInteger(body, "stock", 0, MAX_STOCK, "stock", result.stock, error)) return error;
    if (!readInteger(body, "lowStockThreshold", 0, MAX_STOCK, "low-stock threshold", result.lowStockThreshold, error)) return error;

    result.price = roundMoney(result.price);
    result.purchasePrice = roundMoney(result.purchasePrice);
    if (result.name.empty()) return "Product name cannot be empty.";
    if (result.price <= 0 || result.price >= MAX_PRICE) return "Invalid price value.";
    if (result.stock < 0) return "Invalid stock value.";
    if (result.category.empty()) result.category = "General";
    if (result.unit.empty()) result.unit = "piece";
    return "";
}

bool productNameTaken(const string& name, const string& ignoreId) {
    return any_of(products.begin(), products.end(), [&](const Product& p) {
        return p.id != ignoreId && toLower(p.name) == toLower(name);
    });
}

Response handleCreateProduct(const string& bodyText) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    Product p;
    string error = readProductFields(bodyText, Product(), p);
    if (!error.empty()) return errorResponse(400, error);
    if (productNameTaken(p.name, "")) return errorResponse(409, "A product with this name already exists.");

    p.id = generateProductId();
    products.push_back(p);
    if (!saveProducts()) {
        products.pop_back();
        return errorResponse(500, "Unable to save products file.");
    }
    return jsonResponse(201, productToJson(p));
}

Response handleUpdateProduct(const string& path, const string& bodyText) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    string id = path.substr(string("/api/products/").size());
    Product* p = findProductById(id);
    if (!p) return errorResponse(404, "Product not found.");

    Product updated;
    string error = readProductFields(bodyText, *p, updated);
    if (!error.empty()) return errorResponse(400, error);
    if (productNameTaken(updated.name, id)) return errorResponse(409, "Another product already uses this name.");

    Product previous = *p;
    *p = updated;
    if (!saveProducts()) {
        *p = previous;
        return errorResponse(500, "Unable to save products file.");
    }
    return jsonResponse(200, productToJson(*p));
}

Response handleDeleteProduct(const string& path) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    string id = path.substr(string("/api/products/").size());
    auto it = find_if(products.begin(), products.end(), [&](const Product& p) {
        return p.id == id;
    });
    if (it == products.end()) return errorResponse(404, "Product not found.");

    Product removed = *it;
    size_t position = it - products.begin();
    products.erase(it);
    if (!saveProducts()) {
        products.insert(products.begin() + position, removed);
        return errorResponse(500, "Unable to save products file.");
    }
    return jsonResponse(200, json{{"message", "Product deleted."}});
}

Response handleGetShop() {
    return jsonResponse(200, shopToJson());
}

Response handleGetSales() {
    json list = json::array();
    for (auto& sale : loadSales()) list.push_back(move(sale));
    return jsonResponse(200, list);
}

Response handleUpdateShop(const string& bodyText) {
    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    string name, phone, address;
    if (!readText(body, "shopName", MAX_SHOP_FIELD_LENGTH, "Shop name", name, error)) return errorResponse(400, error);
    if (!readText(body, "shopPhone", MAX_PHONE_LENGTH, "Phone number", phone, error)) return errorResponse(400, error);
    if (!readText(body, "shopAddress", MAX_SHOP_FIELD_LENGTH, "Address", address, error)) return errorResponse(400, error);

    shopName = name.empty() ? "MY SHOP NAME" : name;
    shopPhone = phone;
    shopAddress = address;
    if (!saveShopName()) return errorResponse(500, "Unable to save shop details.");
    return jsonResponse(200, shopToJson());
}

Response handleSale(const string& bodyText) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);
    if (!body.contains("items") || !body["items"].is_array() || body["items"].empty()) {
        return errorResponse(400, "Invalid sale payload.");
    }
    if (body["items"].size() > MAX_BILL_ROWS) {
        return errorResponse(400, "A bill can have at most " + to_string(MAX_BILL_ROWS) + " rows.");
    }

    // Merge repeated products so the stock check sees the full quantity.
    vector<pair<string, int>> saleItems;
    for (const auto& item : body["items"]) {
        if (!item.is_object() || !item.contains("productId") || !item["productId"].is_string()) {
            return errorResponse(400, "Invalid sale payload.");
        }
        int qty = 0;
        if (!item.contains("qty") || !readInteger(item, "qty", 1, MAX_QTY, "quantity", qty, error)) {
            return errorResponse(400, "Invalid quantity for a bill item.");
        }
        string productId = item["productId"].get<string>();
        if (!findProductById(productId)) return errorResponse(404, "One or more products were not found.");

        auto existing = find_if(saleItems.begin(), saleItems.end(), [&](const pair<string, int>& entry) {
            return entry.first == productId;
        });
        if (existing == saleItems.end()) {
            saleItems.push_back({productId, qty});
        } else if (existing->second > MAX_QTY - qty) {
            return errorResponse(400, "Invalid quantity for a bill item.");
        } else {
            existing->second += qty;
        }
    }

    double discount = 0, taxRate = 0, amountPaid = 0;
    string paymentMethod, customerName, customerPhone, customerId;
    if (!readNumber(body, "discount", 0, MAX_PAYMENT, "discount", discount, error)
        || !readNumber(body, "taxRate", 0, MAX_TAX_RATE, "tax rate", taxRate, error)
        || !readNumber(body, "amountPaid", 0, MAX_PAYMENT, "amount paid", amountPaid, error)
        || !readText(body, "paymentMethod", MAX_LABEL_LENGTH, "Payment method", paymentMethod, error)
        || !readText(body, "customerName", MAX_CUSTOMER_FIELD_LENGTH, "Customer name", customerName, error)
        || !readText(body, "customerPhone", MAX_PHONE_LENGTH, "Customer phone", customerPhone, error)
        || !readText(body, "customerId", MAX_ID_LENGTH, "Customer", customerId, error)) {
        return errorResponse(400, error);
    }
    if (paymentMethod != "Cash" && paymentMethod != "Card" && paymentMethod != "Mobile" && paymentMethod != "Credit") {
        paymentMethod = "Cash";
    }

    // A credit sale is only allowed against a known customer record - that's what makes
    // the resulting balance trackable and collectible, rather than an untraceable IOU.
    // When one is linked, its name/phone become canonical for this bill (instead of
    // whatever free text the client sent) so the receipt always matches the real account.
    Customer* creditCustomer = nullptr;
    if (!customerId.empty()) {
        creditCustomer = findCustomerById(customerId);
        if (!creditCustomer) return errorResponse(404, "Customer not found.");
        customerName = creditCustomer->name;
        customerPhone = creditCustomer->phone;
    }
    if (paymentMethod == "Credit" && !creditCustomer) {
        return errorResponse(400, "A credit sale needs a customer to bill it to.");
    }

    json itemsJson = json::array();
    double total = 0.0;
    double grossProfit = 0.0;
    for (const auto& saleItem : saleItems) {
        Product* p = findProductById(saleItem.first);
        int qty = saleItem.second;
        if (qty > p->stock) return errorResponse(400, "Insufficient stock for " + p->name);

        double itemTotal = roundMoney(p->price * qty);
        double itemProfit = roundMoney((p->price - p->purchasePrice) * qty);
        total += itemTotal;
        grossProfit += itemProfit;
        itemsJson.push_back(json{
            {"productId", p->id},
            {"name", p->name},
            {"rate", p->price},
            {"purchasePrice", p->purchasePrice},
            {"qty", qty},
            {"total", itemTotal},
            {"profit", itemProfit},
        });
    }

    total = roundMoney(total);
    discount = min(roundMoney(discount), total);
    amountPaid = roundMoney(amountPaid);
    double taxableSubtotal = total - discount;
    double tax = roundMoney(taxableSubtotal * (taxRate / 100.0));
    double finalTotal = roundMoney(taxableSubtotal + tax);
    // Walk-in sales (no linked customer) must still be paid in full; a credit customer
    // can be billed for less than the total, and the shortfall becomes their balance.
    if (amountPaid + 0.001 < finalTotal && !creditCustomer) {
        return errorResponse(400, "Amount paid is less than the final bill total.");
    }
    double change = roundMoney(amountPaid - finalTotal);
    double profit = roundMoney(grossProfit - discount);

    static unsigned long saleCounter = 0;
    string saleId = "sale_" + to_string(time(nullptr)) + "_" + to_string(++saleCounter);
    int billNumber = nextBillNumber;
    json sale{
        {"id", saleId},
        {"billNo", billNumber},
        {"createdAt", currentTimestamp()},
        {"shopName", shopName},
        {"items", itemsJson},
        {"shopPhone", shopPhone},
        {"shopAddress", shopAddress},
        {"paymentMethod", paymentMethod},
        {"customerName", customerName},
        {"customerPhone", customerPhone},
        {"customerId", customerId},
        {"subtotal", total},
        {"profit", profit},
        {"discount", discount},
        {"taxRate", taxRate},
        {"tax", tax},
        {"amountPaid", amountPaid},
        {"change", change},
        {"total", finalTotal},
    };
    string saleRecord = dumpJson(sale);

    // Save the bill first; stock is only reduced once the bill is safely on disk.
    if (!saveSale(saleRecord)) return errorResponse(500, "Unable to save bill history.");
    ++nextBillNumber;

    for (const auto& saleItem : saleItems) {
        findProductById(saleItem.first)->stock -= saleItem.second;
    }
    if (!saveProducts()) {
        cerr << "Warning: bill " << billNumber << " was saved but updated stock could not be written." << endl;
    }

    return {200, saleRecord};
}

// Adds more items to a bill that was already saved, e.g. a customer comes back to the
// counter before leaving. Recomputes totals from scratch (same formula as handleSale) and
// only deducts stock for the newly added quantities - the original items already reduced
// stock when the bill was first made.
Response handleAddItemsToSale(const string& path, const string& bodyText) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    string id = path.substr(string("/api/sale/").size());
    if (id.empty()) return errorResponse(404, "Bill not found.");

    vector<json> sales = loadSales();
    auto saleIt = find_if(sales.begin(), sales.end(), [&](const json& s) {
        return s.contains("id") && s["id"].is_string() && s["id"].get<string>() == id;
    });
    if (saleIt == sales.end()) return errorResponse(404, "Bill not found.");
    if (saleIt->value("voided", false)) {
        return errorResponse(409, "This bill has been voided and can no longer be edited.");
    }

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    // Items are optional here: an edit that only changes the discount/tax/amount paid
    // (no new products) sends an empty or missing items array.
    vector<pair<string, int>> saleItems;
    if (body.contains("items") && !body["items"].is_null()) {
        if (!body["items"].is_array()) return errorResponse(400, "Invalid sale payload.");
        if (body["items"].size() > MAX_BILL_ROWS) {
            return errorResponse(400, "A bill can have at most " + to_string(MAX_BILL_ROWS) + " rows.");
        }

        // Merge repeated products so the stock check sees the full quantity.
        for (const auto& item : body["items"]) {
            if (!item.is_object() || !item.contains("productId") || !item["productId"].is_string()) {
                return errorResponse(400, "Invalid sale payload.");
            }
            int qty = 0;
            if (!item.contains("qty") || !readInteger(item, "qty", 1, MAX_QTY, "quantity", qty, error)) {
                return errorResponse(400, "Invalid quantity for a bill item.");
            }
            string productId = item["productId"].get<string>();
            if (!findProductById(productId)) return errorResponse(404, "One or more products were not found.");

            auto existing = find_if(saleItems.begin(), saleItems.end(), [&](const pair<string, int>& entry) {
                return entry.first == productId;
            });
            if (existing == saleItems.end()) {
                saleItems.push_back({productId, qty});
            } else if (existing->second > MAX_QTY - qty) {
                return errorResponse(400, "Invalid quantity for a bill item.");
            } else {
                existing->second += qty;
            }
        }
    }

    json& sale = *saleIt;
    double amountPaid = 0;
    // Discount/tax default to whatever the bill already had, but the cashier can adjust
    // either one for the whole bill (not just the newly added rows) by sending a new value.
    double discount = sale.value("discount", 0.0);
    double taxRate = sale.value("taxRate", 0.0);
    if (!readNumber(body, "amountPaid", 0, MAX_PAYMENT, "amount paid", amountPaid, error)
        || !readNumber(body, "discount", 0, MAX_PAYMENT, "discount", discount, error)
        || !readNumber(body, "taxRate", 0, MAX_TAX_RATE, "tax rate", taxRate, error)) {
        return errorResponse(400, error);
    }

    for (const auto& saleItem : saleItems) {
        Product* p = findProductById(saleItem.first);
        if (saleItem.second > p->stock) return errorResponse(400, "Insufficient stock for " + p->name);
    }

    double addedTotal = 0.0;
    for (const auto& saleItem : saleItems) {
        Product* p = findProductById(saleItem.first);
        int qty = saleItem.second;
        double itemTotal = roundMoney(p->price * qty);
        double itemProfit = roundMoney((p->price - p->purchasePrice) * qty);
        addedTotal += itemTotal;
        sale["items"].push_back(json{
            {"productId", p->id},
            {"name", p->name},
            {"rate", p->price},
            {"purchasePrice", p->purchasePrice},
            {"qty", qty},
            {"total", itemTotal},
            {"profit", itemProfit},
        });
    }

    double newSubtotal = roundMoney(sale.value("subtotal", 0.0) + addedTotal);
    discount = min(roundMoney(discount), newSubtotal);
    double taxableSubtotal = newSubtotal - discount;
    double tax = roundMoney(taxableSubtotal * (taxRate / 100.0));
    double finalTotal = roundMoney(taxableSubtotal + tax);
    // Same exception as handleSale: a bill already linked to a credit customer can carry
    // a balance, so adding items to it doesn't suddenly demand full payment.
    bool isCreditSale = !sale.value("customerId", "").empty();
    if (amountPaid + 0.001 < finalTotal && !isCreditSale) {
        return errorResponse(400, "Amount paid is less than the updated bill total.");
    }
    double change = roundMoney(amountPaid - finalTotal);

    double grossProfit = 0.0;
    for (const auto& itemJson : sale["items"]) grossProfit += itemJson.value("profit", 0.0);

    sale["subtotal"] = newSubtotal;
    sale["discount"] = discount;
    sale["taxRate"] = taxRate;
    sale["tax"] = tax;
    sale["total"] = finalTotal;
    sale["amountPaid"] = amountPaid;
    sale["change"] = change;
    sale["profit"] = roundMoney(grossProfit - discount);
    sale["updatedAt"] = currentTimestamp();

    // Save the updated history first; stock is only reduced once it is safely on disk.
    if (!saveAllSales(sales)) return errorResponse(500, "Unable to update bill history.");

    for (const auto& saleItem : saleItems) {
        findProductById(saleItem.first)->stock -= saleItem.second;
    }
    if (!saveProducts()) {
        cerr << "Warning: bill " << sale.value("billNo", 0) << " was updated but stock could not be written." << endl;
    }

    return jsonResponse(200, sale);
}

// Cancels a saved bill (a mistaken sale, a full return, ...): restores stock for every
// line on it and marks it voided so it drops out of revenue/profit, but keeps the record
// itself for the books - sales history is never deleted, only flagged.
Response handleVoidSale(const string& path, const string& bodyText) {
    if (productsFileUnreadable) return unreadableProductsResponse();

    // path is "/api/sale/<id>/void" - strip the fixed prefix and suffix to get the id.
    string id = path.substr(string("/api/sale/").size());
    id = id.substr(0, id.size() - string("/void").size());
    if (id.empty()) return errorResponse(404, "Bill not found.");

    vector<json> sales = loadSales();
    auto saleIt = find_if(sales.begin(), sales.end(), [&](const json& s) {
        return s.contains("id") && s["id"].is_string() && s["id"].get<string>() == id;
    });
    if (saleIt == sales.end()) return errorResponse(404, "Bill not found.");
    if (saleIt->value("voided", false)) return errorResponse(409, "This bill has already been voided.");

    json body;
    string error;
    string reason;
    if (!bodyText.empty()) {
        if (!parseBody(bodyText, body, error)) return errorResponse(400, error);
        if (!readText(body, "reason", MAX_LABEL_LENGTH, "Reason", reason, error)) return errorResponse(400, error);
    }

    json& sale = *saleIt;

    // Restock every line. Older bills (saved before this field existed) have no
    // productId, so those fall back to matching the product by its current name -
    // if the product was since renamed or deleted, that line is skipped and reported
    // back rather than failing the whole void.
    json restockWarnings = json::array();
    for (auto& item : sale["items"]) {
        Product* p = nullptr;
        if (item.contains("productId") && item["productId"].is_string()) {
            p = findProductById(item["productId"].get<string>());
        }
        if (!p && item.contains("name") && item["name"].is_string()) {
            string name = item["name"].get<string>();
            auto it = find_if(products.begin(), products.end(), [&](const Product& product) {
                return product.name == name;
            });
            if (it != products.end()) p = &(*it);
        }
        if (p) {
            p->stock += item.value("qty", 0);
        } else {
            restockWarnings.push_back(item.value("name", "?"));
        }
    }

    sale["voided"] = true;
    sale["voidedAt"] = currentTimestamp();
    if (!reason.empty()) sale["voidReason"] = reason;

    // Save the voided status first; stock is only restored once it is safely on disk.
    if (!saveAllSales(sales)) return errorResponse(500, "Unable to update bill history.");
    if (!saveProducts()) {
        cerr << "Warning: bill " << sale.value("billNo", 0) << " was voided but restocking could not be written." << endl;
    }

    json result = sale;
    if (!restockWarnings.empty()) result["restockWarnings"] = restockWarnings;
    return jsonResponse(200, result);
}

// ---------------------------------------------------------------- credit customers ("khata")

json customerToJson(const Customer& cust, double balance) {
    return json{{"id", cust.id}, {"name", cust.name}, {"phone", cust.phone}, {"createdAt", cust.createdAt}, {"balance", balance}};
}

// A customer's balance is never stored - it's recomputed here every time from the full
// history: every non-voided sale linked to them (total - amountPaid, i.e. what they still
// owe on that bill) minus every repayment recorded against them. This can't drift out of
// sync the way a running counter could if a write were ever missed.
Response handleGetCustomers() {
    map<string, double> dueByCustomer;
    for (const auto& sale : loadSales()) {
        if (sale.value("voided", false)) continue;
        string customerId = sale.value("customerId", "");
        if (customerId.empty()) continue;
        dueByCustomer[customerId] += sale.value("total", 0.0) - sale.value("amountPaid", 0.0);
    }
    for (const auto& payment : loadCustomerPayments()) {
        string customerId = payment.value("customerId", "");
        if (customerId.empty()) continue;
        dueByCustomer[customerId] -= payment.value("amount", 0.0);
    }

    json list = json::array();
    for (const auto& cust : customers) {
        auto it = dueByCustomer.find(cust.id);
        double balance = roundMoney(it != dueByCustomer.end() ? it->second : 0.0);
        list.push_back(customerToJson(cust, balance));
    }
    return jsonResponse(200, list);
}

Response handleCreateCustomer(const string& bodyText) {
    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    string name;
    if (!readText(body, "name", MAX_CUSTOMER_FIELD_LENGTH, "Customer name", name, error)) return errorResponse(400, error);
    if (name.empty()) return errorResponse(400, "Please enter a customer name.");

    string phone;
    if (!readText(body, "phone", MAX_PHONE_LENGTH, "Phone number", phone, error)) return errorResponse(400, error);

    if (any_of(customers.begin(), customers.end(), [&](const Customer& c) { return toLower(c.name) == toLower(name); })) {
        return errorResponse(409, "A customer with this name already exists.");
    }

    Customer cust;
    cust.id = generateCustomerId();
    cust.name = name;
    cust.phone = phone;
    cust.createdAt = currentTimestamp();
    customers.push_back(cust);
    if (!saveCustomers()) {
        customers.pop_back();
        return errorResponse(500, "Unable to save customer.");
    }
    return jsonResponse(201, customerToJson(cust, 0.0));
}

Response handleUpdateCustomer(const string& path, const string& bodyText) {
    string id = path.substr(string("/api/customers/").size());
    Customer* cust = findCustomerById(id);
    if (!cust) return errorResponse(404, "Customer not found.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    string name = cust->name, phone = cust->phone;
    if (!readText(body, "name", MAX_CUSTOMER_FIELD_LENGTH, "Customer name", name, error)) return errorResponse(400, error);
    if (name.empty()) return errorResponse(400, "Please enter a customer name.");
    if (!readText(body, "phone", MAX_PHONE_LENGTH, "Phone number", phone, error)) return errorResponse(400, error);

    if (any_of(customers.begin(), customers.end(), [&](const Customer& c) { return c.id != id && toLower(c.name) == toLower(name); })) {
        return errorResponse(409, "Another customer already uses this name.");
    }

    Customer previous = *cust;
    cust->name = name;
    cust->phone = phone;
    if (!saveCustomers()) {
        *cust = previous;
        return errorResponse(500, "Unable to save customer.");
    }
    return jsonResponse(200, json{{"id", cust->id}, {"name", cust->name}, {"phone", cust->phone}, {"createdAt", cust->createdAt}});
}

// Records a repayment against a customer's running balance (see handleGetCustomers). This
// never touches a sale - it's its own append-only ledger entry, same spirit as sales.jsonl.
Response handleRecordCustomerPayment(const string& path, const string& bodyText) {
    string id = path.substr(string("/api/customers/").size());
    id = id.substr(0, id.size() - string("/payment").size());
    Customer* cust = findCustomerById(id);
    if (!cust) return errorResponse(404, "Customer not found.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    double amount = 0;
    if (!readNumber(body, "amount", 0, MAX_PAYMENT, "amount", amount, error)) return errorResponse(400, error);
    amount = roundMoney(amount);
    if (amount <= 0) return errorResponse(400, "Please enter a payment amount greater than zero.");

    string note;
    if (!readText(body, "note", MAX_CUSTOMER_FIELD_LENGTH, "Note", note, error)) return errorResponse(400, error);

    static unsigned long paymentCounter = 0;
    json payment{
        {"id", "payment_" + to_string(time(nullptr)) + "_" + to_string(++paymentCounter)},
        {"customerId", cust->id},
        {"amount", amount},
        {"note", note},
        {"createdAt", currentTimestamp()},
    };
    if (!appendCustomerPayment(payment)) return errorResponse(500, "Unable to save payment.");

    return jsonResponse(200, payment);
}

// ---------------------------------------------------------------- auth

Response handleAuthStatus() {
    return jsonResponse(200, json{{"configured", authConfigured}});
}

string newSessionToken() {
    string token = randomHex(32);
    sessions[token] = time(nullptr) + SESSION_DURATION_SECONDS;
    return token;
}

// Trims and lowercases a security-question answer before hashing/comparing, so "Rex",
// " rex ", and "REX" are all treated as the same answer - the shop owner should not get
// locked out of recovery over capitalization they don't remember exactly.
string normalizeAnswer(const string& answer) {
    return toLower(trim(answer));
}

// Opens only once: refused the moment a username/password already exists, so a stranger
// who finds the page cannot overwrite the shop's real login. A security question/answer
// is required at the same time, so there's always a recovery path from day one.
Response handleAuthSetup(const string& bodyText) {
    if (authConfigured) return errorResponse(409, "A username and password are already configured.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    string username;
    if (!readText(body, "username", MAX_USERNAME_LENGTH, "Username", username, error)) return errorResponse(400, error);
    if (username.empty()) return errorResponse(400, "Please enter a username.");

    if (!body.contains("password") || !body["password"].is_string()) return errorResponse(400, "Please enter a password.");
    string password = body["password"].get<string>();
    if (password.size() < MIN_PASSWORD_LENGTH) {
        return errorResponse(400, "Password must be at least " + to_string(MIN_PASSWORD_LENGTH) + " characters.");
    }
    if (password.size() > MAX_PASSWORD_LENGTH) return errorResponse(400, "Password is too long.");

    string securityQuestion;
    if (!readText(body, "securityQuestion", MAX_SECURITY_QUESTION_LENGTH, "Security question", securityQuestion, error)) {
        return errorResponse(400, error);
    }
    if (securityQuestion.empty()) return errorResponse(400, "Please enter a security question.");

    if (!body.contains("securityAnswer") || !body["securityAnswer"].is_string()) {
        return errorResponse(400, "Please enter an answer to your security question.");
    }
    string securityAnswer = normalizeAnswer(body["securityAnswer"].get<string>());
    if (securityAnswer.size() < MIN_SECURITY_ANSWER_LENGTH) return errorResponse(400, "Security answer is too short.");
    if (securityAnswer.size() > MAX_SECURITY_ANSWER_LENGTH) return errorResponse(400, "Security answer is too long.");

    string salt = randomHex(16);
    string hash = sha256Hex(salt + password);
    string answerSalt = randomHex(16);
    string answerHash = sha256Hex(answerSalt + securityAnswer);

    string previousUsername = authUsername, previousSalt = authSaltHex, previousHash = authHashHex;
    string previousQuestion = recoveryQuestion, previousAnswerSalt = recoveryAnswerSaltHex, previousAnswerHash = recoveryAnswerHashHex;
    bool previousConfigured = authConfigured, previousRecoveryConfigured = recoveryConfigured;
    authUsername = username;
    authSaltHex = salt;
    authHashHex = hash;
    authConfigured = true;
    recoveryQuestion = securityQuestion;
    recoveryAnswerSaltHex = answerSalt;
    recoveryAnswerHashHex = answerHash;
    recoveryConfigured = true;
    if (!saveAuth()) {
        authUsername = previousUsername;
        authSaltHex = previousSalt;
        authHashHex = previousHash;
        authConfigured = previousConfigured;
        recoveryQuestion = previousQuestion;
        recoveryAnswerSaltHex = previousAnswerSalt;
        recoveryAnswerHashHex = previousAnswerHash;
        recoveryConfigured = previousRecoveryConfigured;
        return errorResponse(500, "Unable to save login details.");
    }

    return jsonResponse(201, json{{"token", newSessionToken()}, {"username", authUsername}});
}

Response handleAuthLogin(const string& bodyText) {
    if (!authConfigured) return errorResponse(409, "No login is configured yet.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    string username;
    if (!readText(body, "username", MAX_USERNAME_LENGTH, "Username", username, error)) return errorResponse(400, error);
    if (!body.contains("password") || !body["password"].is_string()) return errorResponse(400, "Please enter a password.");
    string password = body["password"].get<string>();

    string attemptedHash = sha256Hex(authSaltHex + password);
    if (username != authUsername || !constantTimeEquals(attemptedHash, authHashHex)) {
        return errorResponse(401, "Invalid username or password.");
    }

    return jsonResponse(200, json{{"token", newSessionToken()}, {"username", authUsername}});
}

// Public (no session needed - that's the whole point): lets the login screen show the
// question text itself before anyone has proven who they are. Only the question is
// exposed here, never the answer or its hash.
Response handleAuthRecoveryQuestion() {
    if (!authConfigured || !recoveryConfigured) return jsonResponse(200, json{{"available", false}});
    return jsonResponse(200, json{{"available", true}, {"question", recoveryQuestion}});
}

// Resets the password after a correct answer to the security question set at signup.
// Like a successful password change, this revokes every existing session and the shop
// owner is handed a fresh one so they land straight back in the app.
Response handleAuthRecover(const string& bodyText) {
    if (!authConfigured || !recoveryConfigured) return errorResponse(409, "No recovery question is configured.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    if (!body.contains("answer") || !body["answer"].is_string()) {
        return errorResponse(400, "Please answer the security question.");
    }
    string attemptedHash = sha256Hex(recoveryAnswerSaltHex + normalizeAnswer(body["answer"].get<string>()));
    if (!constantTimeEquals(attemptedHash, recoveryAnswerHashHex)) {
        return errorResponse(401, "That answer is not correct.");
    }

    if (!body.contains("newPassword") || !body["newPassword"].is_string()) {
        return errorResponse(400, "Please enter a new password.");
    }
    string newPassword = body["newPassword"].get<string>();
    if (newPassword.size() < MIN_PASSWORD_LENGTH) {
        return errorResponse(400, "Password must be at least " + to_string(MIN_PASSWORD_LENGTH) + " characters.");
    }
    if (newPassword.size() > MAX_PASSWORD_LENGTH) return errorResponse(400, "Password is too long.");

    string newSalt = randomHex(16);
    string newHash = sha256Hex(newSalt + newPassword);

    string previousSalt = authSaltHex, previousHash = authHashHex;
    authSaltHex = newSalt;
    authHashHex = newHash;
    if (!saveAuth()) {
        authSaltHex = previousSalt;
        authHashHex = previousHash;
        return errorResponse(500, "Unable to save the new password.");
    }

    sessions.clear();
    return jsonResponse(200, json{{"token", newSessionToken()}, {"username", authUsername}});
}

// Requires an existing session (routed below the auth gate, unlike setup/login), plus the
// current password, so a stranger at an unlocked browser still can't lock the owner out.
Response handleAuthChangePassword(const string& bodyText) {
    if (!authConfigured) return errorResponse(409, "No login is configured yet.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    if (!body.contains("currentPassword") || !body["currentPassword"].is_string()) {
        return errorResponse(400, "Please enter your current password.");
    }
    string currentPassword = body["currentPassword"].get<string>();
    string attemptedHash = sha256Hex(authSaltHex + currentPassword);
    if (!constantTimeEquals(attemptedHash, authHashHex)) {
        // 400, not 401: the session itself is still valid, only the typed password is
        // wrong, and the frontend treats any 401 from apiFetch as "session expired".
        return errorResponse(400, "Current password is incorrect.");
    }

    if (!body.contains("newPassword") || !body["newPassword"].is_string()) {
        return errorResponse(400, "Please enter a new password.");
    }
    string newPassword = body["newPassword"].get<string>();
    if (newPassword.size() < MIN_PASSWORD_LENGTH) {
        return errorResponse(400, "Password must be at least " + to_string(MIN_PASSWORD_LENGTH) + " characters.");
    }
    if (newPassword.size() > MAX_PASSWORD_LENGTH) return errorResponse(400, "Password is too long.");

    string newSalt = randomHex(16);
    string newHash = sha256Hex(newSalt + newPassword);

    string previousSalt = authSaltHex, previousHash = authHashHex;
    authSaltHex = newSalt;
    authHashHex = newHash;
    if (!saveAuth()) {
        authSaltHex = previousSalt;
        authHashHex = previousHash;
        return errorResponse(500, "Unable to save the new password.");
    }

    // The old password is now invalid everywhere, so every existing session (this
    // browser included) is revoked; a fresh token is handed back so the settings
    // page that just changed it doesn't get logged out too.
    sessions.clear();
    return jsonResponse(200, json{{"token", newSessionToken()}, {"username", authUsername}});
}

// Sets or replaces the recovery question/answer from Shop Settings - this is how a shop
// that was set up before this feature existed (or just wants to change its question)
// gets one. Requires the current password, same as change-password, so a stranger at an
// unlocked browser can't quietly plant their own recovery answer. Unlike a password
// change, this doesn't touch the password itself, so existing sessions stay valid.
Response handleSetRecoveryQuestion(const string& bodyText) {
    if (!authConfigured) return errorResponse(409, "No login is configured yet.");

    json body;
    string error;
    if (!parseBody(bodyText, body, error)) return errorResponse(400, error);

    if (!body.contains("currentPassword") || !body["currentPassword"].is_string()) {
        return errorResponse(400, "Please enter your current password.");
    }
    string currentPassword = body["currentPassword"].get<string>();
    string attemptedHash = sha256Hex(authSaltHex + currentPassword);
    if (!constantTimeEquals(attemptedHash, authHashHex)) {
        return errorResponse(400, "Current password is incorrect.");
    }

    string securityQuestion;
    if (!readText(body, "securityQuestion", MAX_SECURITY_QUESTION_LENGTH, "Security question", securityQuestion, error)) {
        return errorResponse(400, error);
    }
    if (securityQuestion.empty()) return errorResponse(400, "Please enter a security question.");

    if (!body.contains("securityAnswer") || !body["securityAnswer"].is_string()) {
        return errorResponse(400, "Please enter an answer to your security question.");
    }
    string securityAnswer = normalizeAnswer(body["securityAnswer"].get<string>());
    if (securityAnswer.size() < MIN_SECURITY_ANSWER_LENGTH) return errorResponse(400, "Security answer is too short.");
    if (securityAnswer.size() > MAX_SECURITY_ANSWER_LENGTH) return errorResponse(400, "Security answer is too long.");

    string answerSalt = randomHex(16);
    string answerHash = sha256Hex(answerSalt + securityAnswer);

    string previousQuestion = recoveryQuestion, previousAnswerSalt = recoveryAnswerSaltHex, previousAnswerHash = recoveryAnswerHashHex;
    bool previousRecoveryConfigured = recoveryConfigured;
    recoveryQuestion = securityQuestion;
    recoveryAnswerSaltHex = answerSalt;
    recoveryAnswerHashHex = answerHash;
    recoveryConfigured = true;
    if (!saveAuth()) {
        recoveryQuestion = previousQuestion;
        recoveryAnswerSaltHex = previousAnswerSalt;
        recoveryAnswerHashHex = previousAnswerHash;
        recoveryConfigured = previousRecoveryConfigured;
        return errorResponse(500, "Unable to save the recovery question.");
    }

    return jsonResponse(200, json{{"question", recoveryQuestion}});
}

string bearerToken(const string& authorizationHeader) {
    const string prefix = "Bearer ";
    if (authorizationHeader.rfind(prefix, 0) != 0) return "";
    return authorizationHeader.substr(prefix.size());
}

bool requireSession(const string& authorizationHeader) {
    string token = bearerToken(authorizationHeader);
    if (token.empty()) return false;
    auto it = sessions.find(token);
    if (it == sessions.end()) return false;
    if (it->second < time(nullptr)) {
        sessions.erase(it);
        return false;
    }
    return true;
}

Response handleAuthLogout(const string& authorizationHeader) {
    sessions.erase(bearerToken(authorizationHeader));
    return jsonResponse(200, json{{"message", "Logged out."}});
}

Response handleStaticFile(const string& path) {
    auto it = STATIC_FILES.find(path);
    if (it == STATIC_FILES.end()) return errorResponse(404, "Route not found.");
    string content;
    if (!readFile(joinPath(APP_DIRECTORY, it->second.first), content)) return errorResponse(404, "Route not found.");
    return {200, content, it->second.second};
}

// ---------------------------------------------------------------- HTTP

string getHeaderValue(const string& header, const string& name) {
    string lowerHeader = toLower(header);
    string needle = "\r\n" + toLower(name) + ":";
    size_t pos = lowerHeader.find(needle);
    if (pos == string::npos) return "";
    size_t valueStart = pos + needle.size();
    size_t lineEnd = header.find("\r\n", valueStart);
    return trim(header.substr(valueStart, lineEnd == string::npos ? string::npos : lineEnd - valueStart));
}

bool hasHeader(const string& header, const string& name) {
    return toLower(header).find("\r\n" + toLower(name) + ":") != string::npos;
}

// Returns the Content-Length header value, 0 when it is missing, or -1 when it is invalid.
long long getContentLength(const string& header) {
    if (!hasHeader(header, "Content-Length")) return 0;
    string lengthText = getHeaderValue(header, "Content-Length");
    if (lengthText.empty() || !all_of(lengthText.begin(), lengthText.end(), ::isdigit) || lengthText.size() > 12) {
        return -1;
    }
    return stoll(lengthText);
}

// The backend only answers requests addressed to this computer by name. This blocks
// DNS-rebinding attacks, where a website points its own domain name at 127.0.0.1.
bool hostAllowed(const string& host) {
    string port = to_string(serverPort);
    static const vector<string> names = {"localhost", "127.0.0.1", "[::1]"};
    for (const auto& name : names) {
        if (host == name || host == name + ":" + port) return true;
    }
    return false;
}

// Web pages allowed to call the API: the page served by this backend, and the
// optional `python -m http.server 3000` setup from the README. Pages opened from
// other websites (and sandboxed "null" origins) are refused, which stops other
// sites from reading or changing shop data through the browser (CSRF).
bool originAllowed(const string& origin) {
    set<string> allowed;
    for (const char* name : {"localhost", "127.0.0.1"}) {
        allowed.insert(string("http://") + name + ":" + to_string(serverPort));
        allowed.insert(string("http://") + name + ":3000");
    }
    return allowed.count(toLower(origin)) > 0;
}

string buildResponse(const Response& response, const string& origin) {
    static const map<int, string> statusMap = {
        {200, "200 OK"},
        {201, "201 Created"},
        {204, "204 No Content"},
        {400, "400 Bad Request"},
        {401, "401 Unauthorized"},
        {403, "403 Forbidden"},
        {404, "404 Not Found"},
        {405, "405 Method Not Allowed"},
        {408, "408 Request Timeout"},
        {409, "409 Conflict"},
        {413, "413 Payload Too Large"},
        {431, "431 Request Header Fields Too Large"},
        {500, "500 Internal Server Error"},
        {503, "503 Service Unavailable"}
    };

    string statusText = "500 Internal Server Error";
    auto it = statusMap.find(response.status);
    if (it != statusMap.end()) {
        statusText = it->second;
    }

    string apiOrigins = "http://localhost:" + to_string(serverPort) + " http://127.0.0.1:" + to_string(serverPort);
    ostringstream oss;
    oss << "HTTP/1.1 " << statusText << "\r\n"
        << "Content-Type: " << response.contentType << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "X-Content-Type-Options: nosniff\r\n"
        << "X-Frame-Options: DENY\r\n"
        << "Referrer-Policy: no-referrer\r\n"
        << "Cross-Origin-Resource-Policy: same-site\r\n"
        << "Cache-Control: no-store\r\n"
        << "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
        << "img-src 'self' data: blob:; connect-src 'self' " << apiOrigins << "; "
        << "frame-ancestors 'none'; base-uri 'none'; form-action 'none'; object-src 'none'\r\n";
    if (!origin.empty() && originAllowed(origin)) {
        oss << "Access-Control-Allow-Origin: " << origin << "\r\n"
            << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            << "Access-Control-Max-Age: 600\r\n";
    }
    oss << "Vary: Origin\r\n"
        << "Connection: close\r\n\r\n"
        << response.body;
    return oss.str();
}

Response routeRequest(const string& method, const string& path, const string& bodyText, const string& authorizationHeader) {
    lock_guard<mutex> lock(dataMutex);

    // These three run before any session exists (status is always public, setup
    // only until a login is configured, login validates one of its own).
    if (path == "/api/auth/status" && method == "GET") return handleAuthStatus();
    if (path == "/api/auth/setup" && method == "POST") return handleAuthSetup(bodyText);
    if (path == "/api/auth/login" && method == "POST") return handleAuthLogin(bodyText);
    if (path == "/api/auth/recovery-question" && method == "GET") return handleAuthRecoveryQuestion();
    if (path == "/api/auth/recover" && method == "POST") return handleAuthRecover(bodyText);
    if (path == "/api/auth/logout" && method == "POST") return handleAuthLogout(authorizationHeader);

    // Every other /api/* route holds shop data, so it requires a valid session token.
    // The page shell itself (index.html, script.js, ...) stays public - script.js shows
    // the login screen and never reaches these calls until it holds a token.
    if (path.rfind("/api/", 0) == 0 && !requireSession(authorizationHeader)) {
        return errorResponse(401, "Please log in to continue.");
    }

    if (path == "/api/products" && method == "GET") return handleGetProducts();
    if (path == "/api/products" && method == "POST") return handleCreateProduct(bodyText);
    if (path == "/api/backup" && method == "POST") return handleBackupNow();
    if (path == "/api/shop" && method == "GET") return handleGetShop();
    if (path == "/api/shop" && method == "PUT") return handleUpdateShop(bodyText);
    if (path == "/api/auth/change-password" && method == "POST") return handleAuthChangePassword(bodyText);
    if (path == "/api/auth/set-recovery-question" && method == "POST") return handleSetRecoveryQuestion(bodyText);
    if (path == "/api/sales" && method == "GET") return handleGetSales();
    if (path == "/api/sale" && method == "POST") return handleSale(bodyText);
    if (path.rfind("/api/sale/", 0) == 0 && path.size() > 5 &&
        path.compare(path.size() - 5, 5, "/void") == 0 && method == "POST") {
        return handleVoidSale(path, bodyText);
    }
    if (path.rfind("/api/sale/", 0) == 0 && method == "POST") return handleAddItemsToSale(path, bodyText);
    if (path.rfind("/api/products/", 0) == 0 && method == "PUT") return handleUpdateProduct(path, bodyText);
    if (path.rfind("/api/products/", 0) == 0 && method == "DELETE") return handleDeleteProduct(path);
    if (path == "/api/customers" && method == "GET") return handleGetCustomers();
    if (path == "/api/customers" && method == "POST") return handleCreateCustomer(bodyText);
    if (path.rfind("/api/customers/", 0) == 0 && path.size() > 8 &&
        path.compare(path.size() - 8, 8, "/payment") == 0 && method == "POST") {
        return handleRecordCustomerPayment(path, bodyText);
    }
    if (path.rfind("/api/customers/", 0) == 0 && method == "PUT") return handleUpdateCustomer(path, bodyText);
    if (path.rfind("/api/", 0) != 0 && (method == "GET" || method == "HEAD")) return handleStaticFile(path);
    return errorResponse(404, "Route not found.");
}

Response handleRequest(const string& request, string& origin) {
    size_t headerEnd = request.find("\r\n\r\n");
    if (headerEnd == string::npos) return errorResponse(400, "Malformed request.");

    string header = request.substr(0, headerEnd);
    size_t lineBreak = header.find("\r\n");
    string firstLine = header.substr(0, lineBreak);

    size_t firstSpace = firstLine.find(' ');
    size_t secondSpace = firstSpace == string::npos ? string::npos : firstLine.find(' ', firstSpace + 1);
    if (firstSpace == string::npos || secondSpace == string::npos) {
        return errorResponse(400, "Malformed request line.");
    }

    string method = firstLine.substr(0, firstSpace);
    string path = firstLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    size_t queryStart = path.find('?');
    if (queryStart != string::npos) {
        path = path.substr(0, queryStart);
    }

    if (!hostAllowed(toLower(getHeaderValue(header, "Host")))) {
        return errorResponse(403, "Requests must be addressed to localhost.");
    }
    if (hasHeader(header, "Origin")) {
        origin = getHeaderValue(header, "Origin");
        if (!originAllowed(origin)) {
            origin.clear();
            return errorResponse(403, "This web page is not allowed to use the shop backend. Open http://localhost:" + to_string(serverPort) + "/ instead.");
        }
    }
    // Browsers mark requests from other websites; refuse them even if Origin was stripped.
    if (toLower(getHeaderValue(header, "Sec-Fetch-Site")) == "cross-site") {
        return errorResponse(403, "Cross-site requests are not allowed.");
    }

    if (method == "OPTIONS") return {204, ""};

    long long contentLength = getContentLength(header);
    if (contentLength < 0) return errorResponse(400, "Invalid Content-Length.");
    size_t bodyStart = headerEnd + 4;
    if (bodyStart + static_cast<size_t>(contentLength) > request.size()) {
        return errorResponse(400, "Request body is incomplete.");
    }
    string bodyText = request.substr(bodyStart, static_cast<size_t>(contentLength));

    return routeRequest(method, path, bodyText, getHeaderValue(header, "Authorization"));
}

void sendAll(SOCKET client, const string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int chunk = send(client, data.data() + sent, static_cast<int>(min<size_t>(data.size() - sent, 64 * 1024)), 0);
        if (chunk <= 0) return;
        sent += static_cast<size_t>(chunk);
    }
}

void handleClient(SOCKET client) {
    using clock = chrono::steady_clock;
    auto deadline = clock::now() + chrono::milliseconds(REQUEST_DEADLINE_MS);

    // Short receive timeout so the overall deadline below is checked regularly. A client
    // that trickles bytes forever (slowloris) is cut off at the deadline.
    DWORD timeoutMs = 1000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    string request;
    char buffer[8192];
    size_t expectedSize = string::npos;
    Response early;
    bool rejected = false;

    while (true) {
        if (clock::now() > deadline) {
            early = errorResponse(408, "Request took too long.");
            rejected = true;
            break;
        }
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received == 0) break;
        if (received < 0) {
            if (WSAGetLastError() == WSAETIMEDOUT) continue;
            break;
        }

        request.append(buffer, received);

        size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd == string::npos) {
            if (request.size() > MAX_HEADER_SIZE) {
                early = errorResponse(431, "Request headers are too large.");
                rejected = true;
                break;
            }
            continue;
        }
        if (expectedSize == string::npos) {
            long long contentLength = getContentLength(request.substr(0, headerEnd));
            if (headerEnd > MAX_HEADER_SIZE) {
                early = errorResponse(431, "Request headers are too large.");
                rejected = true;
                break;
            }
            if (contentLength < 0) {
                early = errorResponse(400, "Invalid Content-Length.");
                rejected = true;
                break;
            }
            if (static_cast<unsigned long long>(contentLength) > MAX_REQUEST_SIZE) {
                early = errorResponse(413, "Request is too large.");
                rejected = true;
                break;
            }
            expectedSize = headerEnd + 4 + static_cast<size_t>(contentLength);
        }
        if (request.size() >= expectedSize) {
            break;
        }
    }

    string origin;
    Response response;
    if (rejected) {
        response = early;
    } else if (!request.empty()) {
        try {
            response = handleRequest(request, origin);
        } catch (const exception& error) {
            cerr << "Request failed: " << error.what() << endl;
            response = errorResponse(500, "Internal server error.");
        }
    }

    if (rejected || !request.empty()) {
        sendAll(client, buildResponse(response, origin));
    }
    shutdown(client, SD_SEND);
    closesocket(client);
}

void runClient(SOCKET client) {
    try {
        handleClient(client);
    } catch (...) {
        closesocket(client);
    }
    --activeClients;
}

int main(int argc, char** argv) {
    if (argc > 1) {
        int port = 0;
        if (!parseInt(argv[1], port) || port < 1 || port > 65535) {
            cerr << "Usage: backend.exe [port]" << endl;
            return 1;
        }
        serverPort = port;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }

    loadProducts();
    loadShopName();
    loadNextBillNumber();
    loadAuth();
    loadCustomers();

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        cerr << "Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }

    // Refuse to share the port, so a second copy of the backend reports an error
    // instead of silently competing with the first one.
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&opt, sizeof(opt));

    // Bound to 127.0.0.1 only: the shop data is never reachable from the network.
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(serverPort));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Bind failed: port " << serverPort << " is already in use (is backend.exe already running?)" << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed" << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    cout << "C++ backend running on http://localhost:" << serverPort << endl;
    cout << "Open http://localhost:" << serverPort << "/ in your browser." << endl;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            Sleep(10);
            continue;
        }

        // Cap the number of connections handled at once so a flood of idle
        // connections cannot exhaust threads and memory.
        if (activeClients >= MAX_ACTIVE_CLIENTS) {
            closesocket(client);
            continue;
        }
        ++activeClients;
        try {
            thread(runClient, client).detach();
        } catch (...) {
            --activeClients;
            closesocket(client);
        }
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
