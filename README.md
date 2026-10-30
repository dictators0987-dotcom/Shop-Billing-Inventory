# Shop Inventory and Billing Management Software

This project is a lightweight desktop-friendly web application for managing shop inventory and generating bills.
A small C++ backend (`backend.exe`) stores all data in plain files next to the executable; the web page talks to it on `http://localhost:8080`.

## Features
- Add, update, and delete products (category, unit, purchase price, low-stock threshold)
- Search products by name, or quick-scan by product ID / exact name (F2)
- Add products to the current bill; repeated products are grouped into one row
- Quantity controls, discount, tax and change calculation
- Generate receipts with shop name, phone and address; print on 58mm / 80mm thermal printers
- Stock reduction after a completed sale
- Previous bills are saved in `sales.jsonl` and remain available after restarting the software
- Dashboard with revenue, profit, best sellers and low-stock alerts
- Automatic backups in the `backups` folder, plus a "Backup Now" button
- Export sales history as CSV

## Run locally
Double-click `run_shop.bat`, or start the backend yourself:

```powershell
.\backend.exe          # optional: .\backend.exe 8081 to use another port
```

Keep that terminal running and open http://localhost:8080/ — the backend serves the web page itself.
(`python -m http.server 3000` and http://localhost:3000 also still work.)
Opening `index.html` directly from the folder no longer works: the backend refuses pages it can't identify, so other websites can't reach your shop data.

## Designs
Shop Settings → Design switches between three looks (remembered per browser):
- **Classic** – the original design
- **Counter** – light workshop look: concrete, graphite and safety yellow (`themes/counter.css`)
- **Fan deck** – green side rail with paint-chip dashboard figures (`themes/fandeck.css`)

You can also open a design directly: http://localhost:8080/?design=counter or ?design=fandeck

## Security
The backend only listens on this computer, only answers pages served from localhost
(blocks CSRF / DNS-rebinding), validates every field with a real JSON parser and size limits,
cuts off slow or oversized requests, keeps the newest 50 backups per file, and sends
security headers (CSP, no-sniff, no framing). Run the test suite against an isolated copy:

```powershell
python tests\security_test.py
```

## Build the backend
Requires MinGW-w64 g++:

```powershell
g++ -std=c++17 -O2 -o backend.exe backend.cpp -lws2_32 -static
```

## Data files
- `products.txt` – products, 8 lines per product (id, name, price, stock, category, unit, low-stock threshold, purchase price)
- `sales.jsonl` – one saved bill per line
- `shop_name.txt` – shop name, phone and address
- `backups/` – timestamped copies made before every change

## Source files
- `index.html` – app layout
- `styles.css` – interface styling
- `script.js` – frontend logic
- `backend.cpp` – HTTP API, file storage and web page serving (uses `include/json.hpp`)
- `themes/` – alternative designs
- `tests/security_test.py` – security and robustness tests
