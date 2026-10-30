#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct Product {
    string id;
    string name;
    double price;
    int stock;
};

struct BillItem {
    string productId;
    int quantity;
};

vector<Product> products;
vector<BillItem> bill;
string shopName = "MY SHOP NAME";

const string PRODUCTS_FILE = "products.txt";
const string SHOP_FILE = "shop_name.txt";

string toLower(const string& value) {
    string result = value;
    transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return result;
}

string generateId(const string& prefix) {
    static int counter = 1;
    return prefix + "_" + to_string(counter++);
}

string formatCurrency(double amount) {
    ostringstream oss;
    oss << "Rs. " << fixed << setprecision(2) << amount;
    return oss.str();
}

Product* findProductById(const string& id) {
    for (auto& product : products) {
        if (product.id == id) {
            return &product;
        }
    }
    return nullptr;
}

BillItem* findBillItemByProductId(const string& productId) {
    for (auto& item : bill) {
        if (item.productId == productId) {
            return &item;
        }
    }
    return nullptr;
}

void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void saveProducts() {
    ofstream out(PRODUCTS_FILE);
    if (!out) {
        cout << "Unable to save product data." << endl;
        return;
    }

    for (const auto& product : products) {
        out << product.id << "\n"
            << product.name << "\n"
            << product.price << "\n"
            << product.stock << "\n";
    }

    out.close();
}

void loadProducts() {
    ifstream in(PRODUCTS_FILE);
    if (!in) {
        products.clear();
        saveProducts();
        return;
    }

    products.clear();
    while (in) {
        Product product;
        if (!getline(in, product.id)) break;
        if (!getline(in, product.name)) break;
        if (!(in >> product.price)) break;
        in.ignore(numeric_limits<streamsize>::max(), '\n');
        if (!(in >> product.stock)) break;
        in.ignore(numeric_limits<streamsize>::max(), '\n');

        if (!product.id.empty() && !product.name.empty()) {
            products.push_back(product);
        }
    }

    if (products.empty()) {
        saveProducts();
    }
}

void saveShopName() {
    ofstream out(SHOP_FILE);
    if (!out) {
        cout << "Unable to save shop name." << endl;
        return;
    }
    out << shopName;
    out.close();
}

void loadShopName() {
    ifstream in(SHOP_FILE);
    if (!in) {
        shopName = "MY SHOP NAME";
        saveShopName();
        return;
    }

    getline(in, shopName);
    if (shopName.empty()) {
        shopName = "MY SHOP NAME";
    }
}

void printInventoryTable() {
    cout << "\n==================================================\n";
    cout << "Products / Inventory\n";
    cout << "==================================================\n";
    if (products.empty()) {
        cout << "No products available.\n";
        return;
    }

    cout << left << setw(20) << "Product" << setw(12) << "Rate" << setw(8) << "Stock" << "Actions\n";
    cout << "--------------------------------------------------\n";

    for (const auto& product : products) {
        cout << left << setw(20) << product.name << setw(12) << formatCurrency(product.price)
             << setw(8) << product.stock << "[Update, Delete]\n";
    }
    cout << "==================================================\n";
}

void printSearchResults(const string& query) {
    vector<Product> matches;
    for (const auto& product : products) {
        if (toLower(product.name).find(toLower(query)) != string::npos) {
            matches.push_back(product);
        }
    }

    cout << "\nSearch Results\n";
    if (matches.empty()) {
        cout << "No matching product found.\n";
        return;
    }

    cout << left << setw(20) << "Product" << setw(12) << "Rate" << setw(8) << "Stock" << "Action\n";
    cout << "--------------------------------------------------\n";

    for (const auto& product : matches) {
        cout << left << setw(20) << product.name << setw(12) << formatCurrency(product.price)
             << setw(8) << product.stock << "Add\n";
    }
}

void addProduct() {
    Product product;
    cout << "Enter product name: ";
    getline(cin, product.name);
    if (product.name.empty()) {
        cout << "Product name cannot be empty.\n";
        return;
    }

    cout << "Enter product rate/price: ";
    string priceInput;
    getline(cin, priceInput);
    try {
        product.price = stod(priceInput);
    } catch (...) {
        cout << "Invalid price entered.\n";
        return;
    }
    if (product.price <= 0) {
        cout << "Price must be greater than zero.\n";
        return;
    }

    cout << "Enter available stock: ";
    string stockInput;
    getline(cin, stockInput);
    try {
        product.stock = stoi(stockInput);
    } catch (...) {
        cout << "Invalid stock quantity.\n";
        return;
    }
    if (product.stock < 0) {
        cout << "Stock cannot be negative.\n";
        return;
    }

    for (const auto& existing : products) {
        if (toLower(existing.name) == toLower(product.name)) {
            cout << "Product with this name already exists.\n";
            return;
        }
    }

    product.id = generateId("prod");
    products.push_back(product);
    saveProducts();
    cout << "Product added successfully.\n";
}

void updateProduct() {
    if (products.empty()) {
        cout << "No products available to update.\n";
        return;
    }

    printInventoryTable();
    cout << "Enter product name to update: ";
    string productName;
    getline(cin, productName);

    Product* product = nullptr;
    for (auto& item : products) {
        if (toLower(item.name) == toLower(productName)) {
            product = &item;
            break;
        }
    }

    if (!product) {
        cout << "Product not found.\n";
        return;
    }

    cout << "Enter new price: ";
    string newPrice;
    getline(cin, newPrice);
    try {
        product->price = stod(newPrice);
    } catch (...) {
        cout << "Invalid price entered.\n";
        return;
    }
    if (product->price <= 0) {
        cout << "Price must be greater than zero.\n";
        return;
    }

    cout << "Enter new stock: ";
    string newStock;
    getline(cin, newStock);
    try {
        product->stock = stoi(newStock);
    } catch (...) {
        cout << "Invalid stock quantity.\n";
        return;
    }
    if (product->stock < 0) {
        cout << "Stock cannot be negative.\n";
        return;
    }

    saveProducts();
    cout << "Product updated successfully.\n";
}

void deleteProduct() {
    if (products.empty()) {
        cout << "No products available to delete.\n";
        return;
    }

    printInventoryTable();
    cout << "Enter product name to delete: ";
    string productName;
    getline(cin, productName);

    auto it = find_if(products.begin(), products.end(), [&](const Product& product) {
        return toLower(product.name) == toLower(productName);
    });

    if (it == products.end()) {
        cout << "Product not found.\n";
        return;
    }

    products.erase(it);
    bill.erase(remove_if(bill.begin(), bill.end(), [&](const BillItem& item) {
        return item.productId == it->id;
    }), bill.end());
    saveProducts();
    cout << "Product deleted successfully.\n";
}

double calculateBillTotal() {
    double total = 0.0;
    for (const auto& item : bill) {
        Product* product = findProductById(item.productId);
        if (product) {
            total += product->price * item.quantity;
        }
    }
    return total;
}

void addToBill() {
    if (products.empty()) {
        cout << "No products available.\n";
        return;
    }

    printSearchResults("");
    cout << "\nEnter product name to add: ";
    string searchName;
    getline(cin, searchName);

    Product* product = nullptr;
    for (auto& item : products) {
        if (toLower(item.name) == toLower(searchName)) {
            product = &item;
            break;
        }
    }

    if (!product) {
        cout << "Product not found.\n";
        return;
    }

    cout << "Enter quantity to add: ";
    string qtyInput;
    getline(cin, qtyInput);
    int quantity = 0;
    try {
        quantity = stoi(qtyInput);
    } catch (...) {
        cout << "Invalid quantity entered.\n";
        return;
    }

    if (quantity <= 0) {
        cout << "Quantity must be greater than zero.\n";
        return;
    }

    BillItem* existing = findBillItemByProductId(product->id);
    int currentQty = existing ? existing->quantity : 0;
    if (currentQty + quantity > product->stock) {
        cout << "Insufficient stock. Available stock: " << product->stock << endl;
        return;
    }

    if (existing) {
        existing->quantity += quantity;
    } else {
        bill.push_back({product->id, quantity});
    }

    cout << "Product added to bill successfully.\n";
}

void increaseBillItem() {
    if (bill.empty()) {
        cout << "Bill is empty.\n";
        return;
    }

    cout << "Current bill items:\n";
    for (const auto& item : bill) {
        Product* product = findProductById(item.productId);
        if (product) {
            cout << product->name << " | Qty: " << item.quantity << "\n";
        }
    }

    cout << "Enter product name to increase quantity: ";
    string productName;
    getline(cin, productName);

    Product* product = nullptr;
    for (auto& item : products) {
        if (toLower(item.name) == toLower(productName)) {
            product = &item;
            break;
        }
    }

    if (!product) {
        cout << "Product not found.\n";
        return;
    }

    BillItem* item = findBillItemByProductId(product->id);
    if (!item) {
        cout << "This product is not in the current bill.\n";
        return;
    }

    if (item->quantity + 1 > product->stock) {
        cout << "Insufficient stock. Available stock: " << product->stock << endl;
        return;
    }

    item->quantity += 1;
    cout << "Quantity increased successfully.\n";
}

void decreaseBillItem() {
    if (bill.empty()) {
        cout << "Bill is empty.\n";
        return;
    }

    cout << "Enter product name to decrease quantity: ";
    string productName;
    getline(cin, productName);

    Product* product = nullptr;
    for (auto& item : products) {
        if (toLower(item.name) == toLower(productName)) {
            product = &item;
            break;
        }
    }

    if (!product) {
        cout << "Product not found.\n";
        return;
    }

    BillItem* item = findBillItemByProductId(product->id);
    if (!item) {
        cout << "This product is not in the current bill.\n";
        return;
    }

    item->quantity -= 1;
    if (item->quantity <= 0) {
        bill.erase(remove_if(bill.begin(), bill.end(), [&](const BillItem& billItem) {
            return billItem.productId == product->id;
        }), bill.end());
    }

    cout << "Quantity decreased successfully.\n";
}

void removeBillItem() {
    if (bill.empty()) {
        cout << "Bill is empty.\n";
        return;
    }

    cout << "Enter product name to remove: ";
    string productName;
    getline(cin, productName);

    Product* product = nullptr;
    for (auto& item : products) {
        if (toLower(item.name) == toLower(productName)) {
            product = &item;
            break;
        }
    }

    if (!product) {
        cout << "Product not found.\n";
        return;
    }

    bill.erase(remove_if(bill.begin(), bill.end(), [&](const BillItem& item) {
        return item.productId == product->id;
    }), bill.end());

    cout << "Product removed from bill.\n";
}

void printCurrentBill() {
    if (bill.empty()) {
        cout << "\nCurrent Bill\nNo products added yet.\n";
        return;
    }

    cout << "\n==================================================\n";
    cout << "Current Bill\n";
    cout << "==================================================\n";
    cout << left << setw(25) << "Product" << setw(12) << "Rate" << setw(10) << "Qty" << "Total\n";
    cout << "--------------------------------------------------\n";

    double grandTotal = 0.0;
    for (const auto& item : bill) {
        Product* product = findProductById(item.productId);
        if (!product) continue;

        double total = product->price * item.quantity;
        grandTotal += total;
        cout << left << setw(25) << product->name << setw(12) << formatCurrency(product->price)
             << setw(10) << item.quantity << formatCurrency(total) << "\n";
    }

    cout << "--------------------------------------------------\n";
    cout << "Grand Total: " << formatCurrency(grandTotal) << "\n";
    cout << "==================================================\n";
}

void generateBill() {
    if (bill.empty()) {
        cout << "No products in the bill to generate.\n";
        return;
    }

    cout << "\n" << shopName << "\n";
    cout << "----------------------------------------\n";
    cout << left << setw(25) << "Product" << setw(10) << "Rate" << setw(6) << "Qty" << "Total\n";

    double grandTotal = 0.0;
    for (const auto& item : bill) {
        Product* product = findProductById(item.productId);
        if (!product) continue;

        double total = product->price * item.quantity;
        grandTotal += total;
        cout << left << setw(25) << product->name << setw(10) << product->price << setw(6) << item.quantity
             << total << "\n";

        for (auto& productEntry : products) {
            if (productEntry.id == product->id) {
                productEntry.stock -= item.quantity;
                break;
            }
        }
    }

    cout << "----------------------------------------\n";
    cout << "TOTAL AMOUNT: " << formatCurrency(grandTotal) << "\n";
    cout << "----------------------------------------\n";

    saveProducts();
    bill.clear();
}

void setShopName() {
    cout << "Enter shop name: ";
    getline(cin, shopName);
    if (shopName.empty()) {
        shopName = "MY SHOP NAME";
    }
    saveShopName();
    cout << "Shop name updated successfully.\n";
}

void showMainMenu() {
    cout << "\n=== Shop Inventory & Billing Management ===\n";
    cout << "1. Add Product\n";
    cout << "2. Update Product\n";
    cout << "3. Delete Product\n";
    cout << "4. Search Products\n";
    cout << "5. Add Product to Bill\n";
    cout << "6. Increase Quantity in Bill\n";
    cout << "7. Decrease Quantity in Bill\n";
    cout << "8. Remove Product from Bill\n";
    cout << "9. Generate Final Bill\n";
    cout << "10. Set Shop Name\n";
    cout << "11. View Current Bill\n";
    cout << "12. View Inventory\n";
    cout << "13. Exit\n";
    cout << "Choose an option: ";
}

int main() {
    loadProducts();
    loadShopName();

    while (true) {
        showMainMenu();
        string optionInput;
        getline(cin, optionInput);

        int choice = 0;
        try {
            choice = stoi(optionInput);
        } catch (...) {
            cout << "Invalid option selected.\n";
            continue;
        }

        switch (choice) {
            case 1:
                addProduct();
                break;
            case 2:
                updateProduct();
                break;
            case 3:
                deleteProduct();
                break;
            case 4: {
                cout << "Enter product name to search: ";
                string query;
                getline(cin, query);
                printSearchResults(query);
                break;
            }
            case 5:
                addToBill();
                break;
            case 6:
                increaseBillItem();
                break;
            case 7:
                decreaseBillItem();
                break;
            case 8:
                removeBillItem();
                break;
            case 9:
                generateBill();
                break;
            case 10:
                setShopName();
                break;
            case 11:
                printCurrentBill();
                break;
            case 12:
                printInventoryTable();
                break;
            case 13:
                cout << "Exiting application.\n";
                return 0;
            default:
                cout << "Invalid option. Please try again.\n";
                break;
        }
    }

    return 0;
}
