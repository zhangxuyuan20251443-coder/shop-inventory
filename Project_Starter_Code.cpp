#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

/*
    Shop Inventory Management System

    程序整体流程：
    1. 启动程序后先尝试从 inventory.txt 读取之前保存的库存。
    2. 不断显示主菜单，让用户选择添加商品、显示库存、更新库存、销售商品、
       生成报告、保存或退出。
    3. Inventory 类负责管理整个库存列表。
    4. Product 类负责保存单个商品的数据和商品自己的操作。
    5. PerishableProduct 继承 Product，用来展示易腐商品的特殊信息。
*/

// 库存数量小于或等于这个值时，商品会被视为低库存。
const int LOW_STOCK_LIMIT = 5;

// 程序退出时，库存数据会保存到这个文件中。
const string INVENTORY_FILE = "inventory.txt";

// 判断一段文本是不是只有空格或 tab。
// 这个函数用于拒绝空白商品名或空白供应商名称。
bool isBlank(const string& text) {
    for (char letter : text) {
        if (letter != ' ' && letter != '\t') {
            return false;
        }
    }

    return true;
}

// 检查日期是否符合简单的 YYYY-MM-DD 格式。
// 这里主要用于演示输入验证：长度、横线位置、数字位置、月份和日期范围都会检查。
bool isValidDateFormat(const string& text) {
    if (text.length() != 10) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(text.length()); i++) {
        if (i == 4 || i == 7) {
            if (text[i] != '-') {
                return false;
            }
        }
        else if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }

    // 取出月份和日期部分，确认它们在合理范围内。
    // 这里没有细分每个月的具体天数，因为项目只需要基本格式验证。
    int month = stoi(text.substr(5, 2));
    int day = stoi(text.substr(8, 2));

    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

// 读取用户输入的一行非空文本。
// 如果用户直接按回车，或者只输入空格，程序会提示重新输入。
string readLine(const string& prompt) {
    string value;

    do {
        cout << prompt;
        getline(cin, value);

        if (value.empty() || isBlank(value)) {
            cout << "Input cannot be empty. Please try again.\n";
        }
    } while (value.empty() || isBlank(value));

    return value;
}

// 专门读取易腐商品的过期日期。
// 它会反复调用 readLine()，直到用户输入符合 YYYY-MM-DD 的日期格式。
string readExpiryDate() {
    string value;

    do {
        value = readLine("Expiry date (format YYYY-MM-DD): ");

        if (!isValidDateFormat(value)) {
            cout << "Please enter the date in YYYY-MM-DD format.\n";
        }
    } while (!isValidDateFormat(value));

    return value;
}

// 读取整数，并拒绝无效输入或过小的数值。
// 这里使用 getline() 先读取整行，再用 stringstream 解析。
// 这样可以拒绝 "abc" 和 "1abc" 这类混合输入，而不是把 "1abc" 错当成 1。
int readInt(const string& prompt, int minimum) {
    int value;
    string line;

    while (true) {
        cout << prompt;
        getline(cin, line);

        stringstream stream(line);

        // 只有当整行刚好是一个整数，并且数值不小于 minimum 时，才接受输入。
        if (stream >> value) {
            stream >> ws;

            if (stream.eof() && value >= minimum) {
                return value;
            }
        }

        cout << "Please enter a whole number of at least " << minimum << ".\n";
    }
}

// 读取指定范围内的菜单整数选项。
// readInt() 负责检查是不是整数和是否达到最小值；
// 这个函数再额外检查是否超过最大值。
int readIntInRange(const string& prompt, int minimum, int maximum) {
    int value;

    while (true) {
        value = readInt(prompt, minimum);

        if (value <= maximum) {
            return value;
        }

        cout << "Please enter a number from " << minimum << " to " << maximum << ".\n";
    }
}

// 读取小数，并拒绝无效输入或过小的数值。
// 价格使用 double，所以这里专门读取和验证小数。
double readDouble(const string& prompt, double minimum) {
    double value;
    string line;

    while (true) {
        cout << prompt;
        getline(cin, line);

        stringstream stream(line);

        // 与 readInt() 一样，必须整行都是合法数字，不能接受 "2.5abc"。
        if (stream >> value) {
            stream >> ws;

            if (stream.eof() && value >= minimum) {
                return value;
            }
        }

        cout << "Please enter a number of at least " << fixed << setprecision(2)
             << minimum << ".\n";
    }
}

// 替换文件分隔符，避免保存商品数据时格式出错。
// 程序用 | 分隔文件中的字段，所以商品名或供应商中如果出现 |，就替换成 /。
string escapeField(const string& text) {
    string result;

    for (char letter : text) {
        if (letter == '|') {
            result += '/';
        }
        else {
            result += letter;
        }
    }

    return result;
}

// 将一行已保存的库存文本拆分成多个商品字段。
// 例如 "General|Milk|2.50|4|Local Dairy|-" 会被拆成 6 个字段。
vector<string> split(const string& text, char delimiter) {
    vector<string> fields;
    string field;
    stringstream stream(text);

    while (getline(stream, field, delimiter)) {
        fields.push_back(field);
    }

    return fields;
}

// Product 类保存一个商店商品的数据和相关操作。
class Product {
private:
    // 每个商品都必须保存这些基本信息。
    string name;
    double price;
    int quantity;
    string supplier;

public:
    // 使用必要的库存信息创建一个商品对象。
    // 构造函数的作用是把用户输入的数据保存到对象的成员变量里。
    Product(string productName, double productPrice, int stockQuantity, string productSupplier) {
        name = productName;
        price = productPrice;
        quantity = stockQuantity;
        supplier = productSupplier;
    }

    virtual ~Product() {
    }

    // 以下 getter 函数用于让其他类安全地读取商品信息。
    string getName() const {
        return name;
    }

    double getPrice() const {
        return price;
    }

    int getQuantity() const {
        return quantity;
    }

    string getSupplier() const {
        return supplier;
    }

    // 以下 setter 函数用于修改商品的基本信息。
    void setName(const string& productName) {
        name = productName;
    }

    // 价格不能是负数，所以修改价格前先做检查。
    void setPrice(double productPrice) {
        if (productPrice >= 0) {
            price = productPrice;
        }
    }

    void setSupplier(const string& productSupplier) {
        supplier = productSupplier;
    }

    // 增加库存。只有正数才会被接受，避免把库存意外减少。
    void addStock(int amount) {
        if (amount > 0) {
            quantity += amount;
        }
    }

    // 只有库存数量足够时才允许销售商品。
    // 返回 true 表示销售成功，返回 false 表示销售失败。
    bool sellItem(int amount) {
        if (amount <= 0) {
            return false;
        }

        if (amount > quantity) {
            return false;
        }

        // 通过检查后才真正减少库存，保证库存不会变成负数。
        quantity -= amount;
        return true;
    }

    // 计算该商品当前库存的总价值。
    double calculateStockValue() const {
        return price * quantity;
    }

    // 判断该商品是否需要出现在低库存报告中。
    bool isLowStock() const {
        return quantity <= LOW_STOCK_LIMIT;
    }

    // getCategory() 和 getExtraInformation() 是虚函数。
    // 普通商品返回默认值，子类可以 override 它们来显示自己的特殊信息。
    virtual string getCategory() const {
        return "General";
    }

    virtual string getExtraInformation() const {
        return "-";
    }

    // 在格式化库存表格中输出一行商品信息。
    // setw() 用来控制列宽，让控制台里的库存表更整齐。
    virtual void display(int index) const {
        cout << left << setw(4) << index
             << setw(20) << name
             << setw(12) << getCategory()
             << "$" << right << setw(8) << fixed << setprecision(2) << price
             << setw(10) << quantity
             << "  " << left << setw(18) << supplier
             << getExtraInformation() << "\n";
    }

    // 将一个商品转换成文本行，方便保存到文件。
    // 文件格式为：类别|名称|价格|数量|供应商|额外信息。
    virtual string toFileLine() const {
        stringstream stream;

        stream << getCategory() << "|"
               << escapeField(name) << "|"
               << fixed << setprecision(2) << price << "|"
               << quantity << "|"
               << escapeField(supplier) << "|"
               << escapeField(getExtraInformation());

        return stream.str();
    }
};

// PerishableProduct 是用于展示继承关系的易腐商品扩展类。
class PerishableProduct : public Product {
private:
    // 易腐商品比普通商品多保存一个过期日期。
    string expiryDate;

public:
    // 子类构造函数先调用 Product 构造函数保存通用字段，
    // 然后再保存自己额外拥有的 expiryDate。
    PerishableProduct(string productName, double productPrice, int stockQuantity,
                      string productSupplier, string productExpiryDate)
        : Product(productName, productPrice, stockQuantity, productSupplier) {
        expiryDate = productExpiryDate;
    }

    // 覆盖父类的分类名称，让库存表显示 Perishable。
    string getCategory() const override {
        return "Perishable";
    }

    // 覆盖父类的额外信息，让库存表显示过期日期。
    string getExtraInformation() const override {
        return "Expiry: " + expiryDate;
    }
};

// Inventory 类管理所有 Product 商品对象的集合。
class Inventory {
private:
    // 使用 unique_ptr<Product> 保存商品对象：
    // 1. 可以同时保存 Product 和 PerishableProduct；
    // 2. 对象由 vector 自动管理，不需要手动 delete。
    vector<unique_ptr<Product> > products;

    // 返回匹配商品名称的下标；如果没有找到则返回 -1。
    // 添加商品时会用它检查是否已经存在同名商品。
    int findProductByName(const string& name) const {
        for (int i = 0; i < static_cast<int>(products.size()); i++) {
            if (products[i]->getName() == name) {
                return i;
            }
        }

        return -1;
    }

    // 显示库存列表，并让用户选择一个商品。
    // updateStock() 和 sellProduct() 都需要先让用户选中某个商品。
    Product* chooseProduct() {
        if (products.empty()) {
            cout << "The inventory is empty.\n";
            return nullptr;
        }

        displayInventory();
        int index = readIntInRange("Enter product number: ", 1, static_cast<int>(products.size()));

        // 用户输入的编号从 1 开始，vector 下标从 0 开始，所以这里要减 1。
        return products[index - 1].get();
    }

public:
    // 向库存中添加一个普通商品或易腐商品。
    // 这个函数负责收集用户输入、检查重复名称、选择商品类型并创建对象。
    void addProduct() {
        cout << "\n--- Add New Product ---\n";

        string name = readLine("Product name: ");

        // 不允许同名商品重复出现，否则更新库存时会分不清要操作哪一个。
        if (findProductByName(name) != -1) {
            cout << "A product with this name already exists. Use update stock instead.\n";
            return;
        }

        double price = readDouble("Price: $", 0.0);
        int quantity = readInt("Quantity in stock: ", 0);
        string supplier = readLine("Supplier: ");

        cout << "Category:\n";
        cout << "1. General product\n";
        cout << "2. Perishable product\n";
        int category = readIntInRange("Choose category: ", 1, 2);

        // 根据用户选择创建不同类型的对象。
        // PerishableProduct 仍然可以存进 vector<unique_ptr<Product>>，这是多态的用法。
        if (category == 2) {
            string expiryDate = readExpiryDate();
            products.push_back(make_unique<PerishableProduct>(name, price, quantity, supplier, expiryDate));
        }
        else {
            products.push_back(make_unique<Product>(name, price, quantity, supplier));
        }

        cout << "Product added successfully.\n";
    }

    // 用结构化表格显示全部商品。
    // 这个函数只负责输出，不修改任何库存数据，所以标记为 const。
    void displayInventory() const {
        cout << "\n--- Full Inventory ---\n";

        if (products.empty()) {
            cout << "No products have been added yet.\n";
            return;
        }

        cout << left << setw(4) << "No."
             << setw(20) << "Name"
             << setw(12) << "Category"
             << setw(10) << "Price"
             << setw(10) << "Quantity"
             << setw(20) << "Supplier"
             << "Notes\n";
        cout << string(88, '-') << "\n";

        for (int i = 0; i < static_cast<int>(products.size()); i++) {
            // 这里会自动调用 Product 或 PerishableProduct 对应的 display() 行为。
            products[i]->display(i + 1);
        }
    }

    // 给已有商品增加库存数量。
    // 先选择商品，再输入要增加的数量，最后调用 Product::addStock()。
    void updateStock() {
        cout << "\n--- Update Stock ---\n";

        Product* product = chooseProduct();

        if (product == nullptr) {
            return;
        }

        // 最小值为 1，说明库存更新必须至少增加 1 件。
        int amount = readInt("Amount to add to stock: ", 1);
        product->addStock(amount);

        cout << "Stock updated. New quantity: " << product->getQuantity() << "\n";
    }

    // 销售商品，并防止库存数量变成负数。
    // 真正的库存检查在 Product::sellItem() 中完成。
    void sellProduct() {
        cout << "\n--- Sell Item ---\n";

        Product* product = chooseProduct();

        if (product == nullptr) {
            return;
        }

        int amount = readInt("Quantity to sell: ", 1);

        if (product->sellItem(amount)) {
            // sellItem() 返回 true，说明库存足够并且已经扣减成功。
            cout << "Sale completed. Remaining stock: " << product->getQuantity() << "\n";

            if (product->isLowStock()) {
                cout << "Low stock alert: this product should be reordered soon.\n";
            }
        }
        else {
            // sellItem() 返回 false，说明库存不足或输入数量不合法。
            cout << "Sale rejected. There is not enough stock available.\n";
        }
    }

    // 显示库存总价值，并列出低库存商品。
    // 总价值 = 每个商品价格 x 当前数量，然后把所有商品相加。
    void generateReport() const {
        cout << "\n--- Inventory Report ---\n";

        if (products.empty()) {
            cout << "No products are available for reporting.\n";
            return;
        }

        double totalValue = 0.0;
        int lowStockCount = 0;

        // 第一次循环：计算总库存价值，并统计低库存商品数量。
        for (const auto& product : products) {
            totalValue += product->calculateStockValue();

            if (product->isLowStock()) {
                lowStockCount++;
            }
        }

        cout << fixed << setprecision(2);
        cout << "Number of products: " << products.size() << "\n";
        cout << "Total value of stock: $" << totalValue << "\n";
        cout << "Low stock limit: " << LOW_STOCK_LIMIT << " units\n";
        cout << "Products currently low in stock: " << lowStockCount << "\n";

        if (lowStockCount > 0) {
            cout << "\nLow stock products:\n";

            // 第二次循环：把具体哪些商品低库存列出来。
            for (const auto& product : products) {
                if (product->isLowStock()) {
                    cout << "- " << product->getName()
                         << " (" << product->getQuantity() << " left, supplier: "
                         << product->getSupplier() << ")\n";
                }
            }
        }
    }

    // 保存当前库存，方便下次运行程序时读取。
    // 每个商品保存为一行文本，字段之间用 | 分隔。
    void saveToFile() const {
        ofstream file(INVENTORY_FILE);

        if (!file) {
            cout << "Could not save inventory file.\n";
            return;
        }

        for (const auto& product : products) {
            // 多态调用：普通商品和易腐商品都可以生成自己的保存文本。
            file << product->toFileLine() << "\n";
        }

        cout << "Inventory saved to " << INVENTORY_FILE << ".\n";
    }

    // 如果库存文件存在，则读取之前保存的商品数据。
    // 读取时会把每一行拆分回 Product 或 PerishableProduct 对象。
    void loadFromFile() {
        ifstream file(INVENTORY_FILE);

        // 第一次运行程序时可能还没有库存文件，这是正常情况，直接返回即可。
        if (!file) {
            return;
        }

        // 重新加载文件前先清空当前列表，避免重复加入旧数据。
        products.clear();
        string line;

        while (getline(file, line)) {
            vector<string> fields = split(line, '|');

            // 至少需要类别、名称、价格、数量、供应商 5 个字段。
            if (fields.size() < 5) {
                continue;
            }

            try {
                // 把文件中的文本字段转换回程序内部使用的数据类型。
                string category = fields[0];
                string name = fields[1];
                double price = stod(fields[2]);
                int quantity = stoi(fields[3]);
                string supplier = fields[4];

                // 跳过明显无效的数据，防止坏文件导致程序崩溃或产生错误库存。
                if ((category != "General" && category != "Perishable") ||
                    isBlank(name) || isBlank(supplier) || price < 0 || quantity < 0) {
                    cout << "Warning: Invalid inventory record skipped.\n";
                    continue;
                }

                if (category == "Perishable" && fields.size() >= 6) {
                    // 易腐商品的第 6 个字段保存了 "Expiry: 日期"。
                    // 读回对象时只保留真正的日期部分。
                    string extra = fields[5];
                    string expiryPrefix = "Expiry: ";

                    if (extra.find(expiryPrefix) == 0) {
                        extra = extra.substr(expiryPrefix.length());
                    }

                    products.push_back(make_unique<PerishableProduct>(name, price, quantity, supplier, extra));
                }
                else {
                    // 普通商品不需要额外日期信息。
                    products.push_back(make_unique<Product>(name, price, quantity, supplier));
                }
            }
            catch (const exception&) {
                // stod() 或 stoi() 转换失败时会进入这里，例如价格写成 abc。
                cout << "Warning: Invalid inventory record skipped.\n";
            }
        }
    }
};

// 向用户显示主菜单选项。
// main() 每轮循环都会调用它，让用户知道可以执行哪些操作。
void showMenu() {
    cout << "\n=== Shop Inventory Management System ===\n";
    cout << "1. Add new product\n";
    cout << "2. Display full inventory\n";
    cout << "3. Update stock level\n";
    cout << "4. Sell item\n";
    cout << "5. Generate report\n";
    cout << "6. Save inventory\n";
    cout << "7. Exit\n";
}

int main() {
    // 创建一个 Inventory 对象，整个程序通过它管理所有商品。
    Inventory inventory;

    // 先读取已保存的数据，然后重复显示菜单直到用户退出。
    inventory.loadFromFile();

    int choice;

    do {
        showMenu();
        choice = readIntInRange("Enter choice: ", 1, 7);

        // 根据用户输入的菜单编号，调用 Inventory 中对应的功能。
        switch (choice) {
        case 1:
            inventory.addProduct();
            break;
        case 2:
            inventory.displayInventory();
            break;
        case 3:
            inventory.updateStock();
            break;
        case 4:
            inventory.sellProduct();
            break;
        case 5:
            inventory.generateReport();
            break;
        case 6:
            inventory.saveToFile();
            break;
        case 7:
            // 退出前自动保存一次，避免用户忘记手动选择 Save inventory。
            inventory.saveToFile();
            cout << "Inventory saved. Exiting program.\n";
            break;
        default:
            cout << "Invalid choice. Please choose an option from 1 to 7.\n";
            break;
        }
    } while (choice != 7);

    // 返回 0 表示程序正常结束。
    return 0;
}
