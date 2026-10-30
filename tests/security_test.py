"""Security and robustness test suite for the shop billing backend.

Runs against an ISOLATED copy of backend.exe (in a temp folder) so the real
products.txt / sales.jsonl are never touched.

Usage:
    python tests/security_test.py                 # tests ./backend.exe on port 18080
    python tests/security_test.py --exe old.exe --port 8080

Covers the usual OWASP-style checks for a local web API:
  access control (CORS, CSRF, DNS rebinding), injection (JSON / file-format /
  XSS payload storage / path traversal), business-logic tampering, numeric
  edge cases, race conditions, DoS (oversized bodies, slowloris, malformed
  input), security headers and frontend static checks.
"""

import argparse
import http.client
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EVIL = "http://evil.example"

results = []


def record(category, name, ok, detail=""):
    results.append((category, name, ok, detail))
    mark = "PASS" if ok else "FAIL"
    print(f"[{mark}] {category}: {name}" + (f"  -- {detail}" if detail and not ok else ""))


class Api:
    def __init__(self, port):
        self.port = port
        self.host = f"localhost:{port}"
        # Set by test_auth() once it logs in; every request below attaches it
        # automatically (every /api/* route except auth itself now requires it).
        self.token = None

    def request(self, method, path, body=None, headers=None, raw_body=None, timeout=5, auth=True):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        hdrs = {"Host": self.host}
        if auth and self.token:
            hdrs["Authorization"] = f"Bearer {self.token}"
        if body is not None:
            raw_body = json.dumps(body).encode()
            hdrs["Content-Type"] = "application/json"
        if headers:
            hdrs.update(headers)
        conn.putrequest(method, path, skip_host=True, skip_accept_encoding=True)
        for k, v in hdrs.items():
            conn.putheader(k, v)
        if raw_body is not None:
            conn.putheader("Content-Length", str(len(raw_body)))
        conn.endheaders(raw_body)
        resp = conn.getresponse()
        data = resp.read()
        result = (resp.status, {k.lower(): v for k, v in resp.getheaders()}, data)
        conn.close()
        return result

    def json(self, method, path, body=None, headers=None, auth=True):
        status, hdrs, data = self.request(method, path, body, headers, auth=auth)
        try:
            parsed = json.loads(data) if data else None
        except ValueError:
            parsed = None
        return status, hdrs, parsed

    def raw(self, payload, timeout=5):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=timeout)
        s.sendall(payload)
        chunks = []
        try:
            while True:
                c = s.recv(65536)
                if not c:
                    break
                chunks.append(c)
        except socket.timeout:
            pass
        s.close()
        return b"".join(chunks)

    def alive(self):
        # /api/auth/status is the one /api/* route that never requires a session, so it
        # works as a liveness probe both before login exists and throughout the suite.
        try:
            status, _, _ = self.request("GET", "/api/auth/status", timeout=3, auth=False)
            return status == 200
        except OSError:
            return False

    def products(self):
        return self.json("GET", "/api/products")[2] or []

    def create(self, **fields):
        body = {"name": "Item", "price": 100, "purchasePrice": 50, "stock": 10,
                "category": "Hardware", "unit": "piece", "lowStockThreshold": 2}
        body.update(fields)
        return self.json("POST", "/api/products", body)

    def sale(self, items, **extra):
        body = {"items": items, "discount": 0, "taxRate": 0, "amountPaid": 10 ** 9,
                "paymentMethod": "Cash", "customerName": "", "customerPhone": ""}
        body.update(extra)
        return self.json("POST", "/api/sale", body)

    def customer(self, **fields):
        body = {"name": "Customer", "phone": "555-0100"}
        body.update(fields)
        return self.json("POST", "/api/customers", body)


def find(api, name):
    return next((p for p in api.products() if p["name"] == name), None)


TEST_USERNAME = "sectest"
TEST_PASSWORD = "sectest-password-123"
TEST_SECURITY_QUESTION = "What is the test fixture's favorite color?"
TEST_SECURITY_ANSWER = "  Blue  "  # deliberately padded/mixed-case to test normalization


# ---------------------------------------------------------------- authentication
# Every other /api/* route now requires a session, so this must run first and leave
# api.token set - every later test relies on it to even reach the thing it's checking.
def test_auth(api):
    c = "Authentication"

    status, _, resp = api.json("GET", "/api/auth/status", auth=False)
    record(c, "Auth status is public and reports not configured yet",
           status == 200 and resp == {"configured": False}, f"status={status} resp={resp}")

    status, _, _ = api.json("GET", "/api/products", auth=False)
    record(c, "Protected route rejects a request with no token", status == 401, f"status={status}")

    status, _, _ = api.request("GET", "/api/products", headers={"Authorization": "Bearer not-a-real-token"})
    record(c, "Protected route rejects a made-up token", status == 401, f"status={status}")

    status, _, resp = api.json("GET", "/api/auth/recovery-question", auth=False)
    record(c, "Recovery question is public and reports unavailable before setup",
           status == 200 and resp == {"available": False}, f"status={status} resp={resp}")

    status, _, _ = api.json("POST", "/api/auth/setup",
                             {"username": TEST_USERNAME, "password": TEST_PASSWORD}, auth=False)
    record(c, "Setup without a security question is rejected", status == 400, f"status={status}")

    status, _, resp = api.json("POST", "/api/auth/setup",
                                {"username": TEST_USERNAME, "password": TEST_PASSWORD,
                                 "securityQuestion": TEST_SECURITY_QUESTION, "securityAnswer": TEST_SECURITY_ANSWER},
                                auth=False)
    ok = status == 201 and isinstance(resp, dict) and "token" in resp
    record(c, "Setting up the first login (with a security question) succeeds and returns a token",
           ok, f"status={status} resp={resp}")
    if not ok:
        raise RuntimeError("auth setup failed, cannot run the rest of the suite")
    api.token = resp["token"]

    status, _, resp = api.json("GET", "/api/auth/recovery-question", auth=False)
    record(c, "Recovery question is public and matches what was set at signup",
           status == 200 and resp == {"available": True, "question": TEST_SECURITY_QUESTION},
           f"status={status} resp={resp}")

    status, _, _ = api.json("POST", "/api/auth/setup",
                             {"username": "someone-else", "password": "another-password"}, auth=False)
    record(c, "A second login cannot be set up once one exists", status == 409, f"status={status}")

    status, _, _ = api.json("GET", "/api/products")
    record(c, "The token from setup grants access to protected routes", status == 200, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/login",
                             {"username": TEST_USERNAME, "password": "wrong-password"}, auth=False)
    record(c, "Login with the wrong password is rejected", status == 401, f"status={status}")

    old_token = api.token
    status, _, _ = api.json("POST", "/api/auth/logout")
    record(c, "Logout succeeds", status == 200, f"status={status}")

    status, _, _ = api.request("GET", "/api/products", headers={"Authorization": f"Bearer {old_token}"})
    record(c, "Token is invalid after logout", status == 401, f"status={status}")

    status, _, resp = api.json("POST", "/api/auth/login",
                                {"username": TEST_USERNAME, "password": TEST_PASSWORD}, auth=False)
    ok = status == 200 and isinstance(resp, dict) and "token" in resp
    record(c, "Can log back in with the right password after logout", ok, f"status={status}")
    if not ok:
        raise RuntimeError("could not log back in, cannot run the rest of the suite")
    api.token = resp["token"]

    status, _, _ = api.json("POST", "/api/auth/change-password",
                             {"currentPassword": "wrong-current-password", "newPassword": "new-password-123"})
    record(c, "Changing password with the wrong current password is rejected (400, not 401)",
           status == 400, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/change-password",
                             {"currentPassword": TEST_PASSWORD, "newPassword": "ab"})
    record(c, "A too-short new password is rejected", status == 400, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/change-password",
                             {"currentPassword": TEST_PASSWORD, "newPassword": "new-test-password-456"}, auth=False)
    record(c, "Change-password requires a valid session", status == 401, f"status={status}")

    new_password = "new-test-password-456"
    status, _, resp = api.json("POST", "/api/auth/change-password",
                                {"currentPassword": TEST_PASSWORD, "newPassword": new_password})
    ok = status == 200 and isinstance(resp, dict) and "token" in resp
    record(c, "Changing password with the correct current password succeeds and returns a fresh token",
           ok, f"status={status}")
    if not ok:
        raise RuntimeError("password change failed, cannot run the rest of the suite")
    changed_token = resp["token"]

    status, _, _ = api.request("GET", "/api/products", headers={"Authorization": f"Bearer {old_token}"})
    record(c, "Every prior session is revoked when the password changes", status == 401, f"status={status}")

    api.token = changed_token
    status, _, _ = api.json("GET", "/api/products")
    record(c, "The fresh token from a password change works immediately", status == 200, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/login",
                             {"username": TEST_USERNAME, "password": new_password}, auth=False)
    record(c, "Can log in with the new password", status == 200, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/login",
                             {"username": TEST_USERNAME, "password": TEST_PASSWORD}, auth=False)
    record(c, "The old password no longer works", status == 401, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/recover",
                             {"answer": "definitely not blue", "newPassword": "doesnt-matter-123"}, auth=False)
    record(c, "Recovering with the wrong answer is rejected", status == 401, f"status={status}")

    status, _, _ = api.request("POST", "/api/auth/recover", headers={"Origin": EVIL},
                                body={"answer": TEST_SECURITY_ANSWER, "newPassword": "doesnt-matter-123"})
    record(c, "Cross-site POST to the recovery endpoint is rejected", status >= 400, f"status={status}")

    recovered_password = "recovered-password-789"
    # Answer is submitted in a different case/spacing than it was set with, to confirm
    # the answer match is normalized rather than an exact byte-for-byte comparison.
    status, _, resp = api.json("POST", "/api/auth/recover",
                                {"answer": "BLUE", "newPassword": recovered_password}, auth=False)
    ok = status == 200 and isinstance(resp, dict) and "token" in resp
    record(c, "Recovering with the right answer (normalized) succeeds and returns a fresh token",
           ok, f"status={status} resp={resp}")
    if not ok:
        raise RuntimeError("password recovery failed, cannot run the rest of the suite")
    recovered_token = resp["token"]

    status, _, _ = api.request("GET", "/api/products", headers={"Authorization": f"Bearer {changed_token}"})
    record(c, "Every prior session is revoked when the password is recovered", status == 401, f"status={status}")

    api.token = recovered_token
    status, _, _ = api.json("GET", "/api/products")
    record(c, "The fresh token from a password recovery works immediately", status == 200, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/login",
                             {"username": TEST_USERNAME, "password": recovered_password}, auth=False)
    record(c, "Can log in with the recovered password", status == 200, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/login",
                             {"username": TEST_USERNAME, "password": new_password}, auth=False)
    record(c, "The password from before recovery no longer works", status == 401, f"status={status}")

    # Changing the recovery question itself - lets a shop set one up after the fact, or
    # replace a forgotten/compromised one. api.token (recovered_token) is still logged in.
    status, _, _ = api.json("POST", "/api/auth/set-recovery-question",
                             {"currentPassword": "wrong-password", "securityQuestion": "New question?", "securityAnswer": "answer"})
    record(c, "Setting a recovery question with the wrong current password is rejected",
           status == 400, f"status={status}")

    status, _, _ = api.json("POST", "/api/auth/set-recovery-question",
                             {"currentPassword": recovered_password, "securityQuestion": "New question?", "securityAnswer": "answer"},
                             auth=False)
    record(c, "Setting a recovery question requires a valid session", status == 401, f"status={status}")

    new_question = "What street did the test shop open on?"
    status, _, resp = api.json("POST", "/api/auth/set-recovery-question",
                                {"currentPassword": recovered_password, "securityQuestion": new_question, "securityAnswer": "Elm Street"})
    record(c, "Setting a recovery question with the correct current password succeeds",
           status == 200 and resp == {"question": new_question}, f"status={status} resp={resp}")

    status, _, resp = api.json("GET", "/api/auth/recovery-question", auth=False)
    record(c, "The public recovery-question endpoint reflects the newly set question",
           status == 200 and resp == {"available": True, "question": new_question}, f"status={status} resp={resp}")

    status, _, _ = api.json("POST", "/api/auth/recover",
                             {"answer": TEST_SECURITY_ANSWER, "newPassword": "shouldnt-work-123"}, auth=False)
    record(c, "The old security answer no longer works once the question is replaced",
           status == 401, f"status={status}")

    status, _, resp = api.json("POST", "/api/auth/recover",
                                {"answer": "elm street", "newPassword": "after-new-question-123"}, auth=False)
    ok = status == 200 and isinstance(resp, dict) and "token" in resp
    record(c, "Recovering with the newly set question/answer works", ok, f"status={status}")
    if not ok:
        raise RuntimeError("final recovery failed, cannot run the rest of the suite")
    # That recovery revoked every session (including the one every later test group
    # relies on), so api.token must move to the fresh one before anything else runs.
    api.token = resp["token"]


# ---------------------------------------------------------------- access control
def test_access_control(api):
    c = "Access control"
    _, hdrs, _ = api.json("GET", "/api/products", headers={"Origin": EVIL})
    acao = hdrs.get("access-control-allow-origin", "")
    record(c, "CORS does not let other websites read data", acao not in ("*", EVIL), f"Access-Control-Allow-Origin: {acao}")

    before = len(api.products())
    status, _, _ = api.request("POST", "/api/products", headers={"Origin": EVIL, "Content-Type": "text/plain"},
                               raw_body=json.dumps({"name": "csrf-item", "price": 1, "stock": 1}).encode())
    created = find(api, "csrf-item") is not None
    record(c, "CSRF: cross-site POST (no preflight) is rejected", not created and status >= 400, f"status {status}, created={created}")

    status, _, _ = api.request("DELETE", "/api/products/product_1", headers={"Origin": EVIL})
    record(c, "CSRF: cross-site DELETE is rejected", status >= 400 and status != 404 or len(api.products()) >= before, f"status {status}")

    status, hdrs, _ = api.request("OPTIONS", "/api/products/x", headers={
        "Origin": EVIL, "Access-Control-Request-Method": "PUT"})
    acao = hdrs.get("access-control-allow-origin", "")
    record(c, "CORS preflight from other website is refused", acao not in ("*", EVIL), f"Access-Control-Allow-Origin: {acao}")

    status, _, _ = api.request("POST", "/api/products", body={"name": "null-origin", "price": 1, "stock": 1},
                               headers={"Origin": "null"})
    record(c, "Sandboxed iframe (Origin: null) cannot write", find(api, "null-origin") is None, f"status {status}")

    status, _, _ = api.request("GET", "/api/products", headers={"Host": "evil.example"})
    record(c, "DNS rebinding: foreign Host header rejected", status >= 400, f"status {status}")

    status, _, _ = api.request("GET", "/api/products", headers={"Origin": "http://localhost:3000"})
    record(c, "Legitimate local origin still works", status == 200, f"status {status}")


# ---------------------------------------------------------------- injection
def test_injection(api):
    c = "Injection"
    xss = '<img src=x onerror=alert(1)><script>alert(2)</script>'
    status, _, body = api.create(name=xss)
    stored = find(api, xss)
    record(c, "XSS payload stored as plain text and returned as valid JSON", stored is not None and stored["name"] == xss, f"status {status}")

    # Value equal to a key name, placed before the real key: naive substring parsers read the wrong field.
    api.request("POST", "/api/products", headers={"Content-Type": "application/json"},
                raw_body=b'{"name":"price","stock":5,"price":100,"purchasePrice":1}')
    p = find(api, "price")
    record(c, "JSON key confusion (value equal to a key name) parsed correctly",
           p is not None and p["price"] == 100 and p["stock"] == 5, f"stored={p}")

    tricky = 'quote" ,"price":0.01,"stock":999999 '
    api.create(name=tricky, price=100, stock=5)
    p = next((x for x in api.products() if x["name"].startswith("quote")), None)
    record(c, "JSON string-escape injection cannot change other fields",
           p is not None and p["price"] == 100 and p["stock"] == 5, f"stored={p}")

    count_before = len(api.products())
    api.create(name="line\nbreak\r\nname", category="cat\negory", unit="u\nnit", price=10, stock=1)
    status, _, _ = api.request("POST", "/api/backup")
    record(c, "Newlines cannot corrupt products.txt record layout",
           all("\n" not in x["name"] and "\n" not in x["category"] for x in api.products()) and len(api.products()) == count_before + 1)

    status, _, body = api.request("GET", "/api/products/../../products.txt")
    record(c, "Path traversal (/api/products/../..) leaks nothing", b"product_" not in body or body.startswith(b"[") or body.startswith(b"{"), f"status {status}")
    for path in ["/../products.txt", "/..%2fproducts.txt", "/%2e%2e/backend.cpp", "/products.txt",
                 "/sales.jsonl", "/backend.cpp", "/shop_name.txt", "/backups/", "/..\\products.txt", "/C:/Windows/win.ini"]:
        status, _, body = api.request("GET", path)
        leaked = status == 200 and (b"product_" in body and not body.startswith(b"<") or b"#include" in body or b"[fonts]" in body or b"shopName" in body and not body.startswith(b"<"))
        record(c, f"Data/source file not exposed: GET {path}", not leaked, f"status {status}")

    pid = api.create(name="saleinj", price=10, stock=50)[2]
    pid = pid["id"] if isinstance(pid, dict) else None
    status, _, sale = api.sale([{"productId": pid, "qty": 1}], customerName='x"}\n{"id":"forged","total":1}',
                               customerPhone="\u2028</script>")
    status, _, sales = api.json("GET", "/api/sales")
    record(c, "Customer fields cannot forge or corrupt sales history",
           isinstance(sales, list) and not any(s.get("id") == "forged" for s in sales), "sales history not valid JSON" if sales is None else "")

    status, _, _ = api.sale([{"productId": pid + '"}]', "qty": 1}])
    record(c, "Malformed productId in sale is rejected cleanly", status in (400, 404), f"status {status}")


# ---------------------------------------------------------------- business logic
def test_business_logic(api):
    c = "Business logic"
    pid = api.create(name="biz", price=100, purchasePrice=60, stock=10)[2]["id"]

    for label, qty in [("negative", -5), ("zero", 0), ("fractional", 0.5), ("string", "3"), ("huge", 10 ** 12), ("NaN", "NaN")]:
        status, _, _ = api.sale([{"productId": pid, "qty": qty}])
        stock = find(api, "biz")["stock"]
        ok = status >= 400 and stock == 10
        record(c, f"Sale quantity {label} rejected", ok, f"status {status}, stock {stock}")

    status, _, _ = api.sale([{"productId": pid, "qty": 2147483647}, {"productId": pid, "qty": 2147483647}])
    record(c, "Integer overflow by repeating an item is rejected", status >= 400 and find(api, "biz")["stock"] == 10, f"status {status}")

    status, _, sale = api.sale([{"productId": pid, "qty": 1, "price": 0.01, "rate": 0.01, "total": 0.01}])
    record(c, "Client cannot set the price of sold items", status == 200 and sale["subtotal"] == 100, f"sale={sale and sale.get('subtotal')}")

    status, _, sale = api.sale([{"productId": pid, "qty": 1}], discount=-500)
    record(c, "Negative discount cannot inflate total", status != 200 or sale["total"] <= 100, f"total={sale and sale.get('total')}")
    status, _, sale = api.sale([{"productId": pid, "qty": 1}], amountPaid=1)
    record(c, "Underpayment rejected", status >= 400, f"status {status}")
    status, _, sale = api.sale([{"productId": pid, "qty": 1}], taxRate=-50)
    record(c, "Negative tax rate rejected or clamped", status != 200 or sale["total"] >= 100, f"total={sale and sale.get('total')}")
    status, _, sale = api.sale([{"productId": pid, "qty": 1}], taxRate=1e300, amountPaid=1e308)
    status2, _, sales = api.json("GET", "/api/sales")
    record(c, "Absurd tax rate cannot write invalid numbers into sales history", isinstance(sales, list) and status >= 400,
           f"sale status {status}, history valid={isinstance(sales, list)}")

    for label, fields in [("negative price", {"price": -1}), ("NaN price", {"price": "NaN"}),
                          ("infinite price", {"price": "1e999"}), ("negative stock", {"stock": -1}),
                          ("stock beyond int range", {"stock": 1e10}), ("negative purchase price", {"purchasePrice": -5}),
                          ("huge purchase price", {"purchasePrice": 1e300}), ("negative threshold", {"lowStockThreshold": -1}),
                          ("100 KB name", {"name": "A" * 100_000}), ("empty name", {"name": "   "})]:
        fields.setdefault("name", f"bad-{label}")
        status, _, _ = api.create(**fields)
        record(c, f"Product with {label} rejected", status == 400, f"status {status}")

    status, _, _ = api.request("PUT", "/api/shop", body={"shopName": "S" * 50_000, "shopPhone": "1", "shopAddress": "a"})
    _, _, shop = api.json("GET", "/api/shop")
    record(c, "Oversized shop name rejected or truncated", status == 400 or len(shop["shopName"]) <= 200, f"len={len(shop['shopName'])}")

    status, _, _ = api.sale([{"productId": pid, "qty": 1}] * 5000)
    record(c, "Bill with thousands of rows handled", api.alive(), f"status {status}")


# ---------------------------------------------------------------- bill editing
def test_bill_edit(api):
    c = "Bill editing (add items to a saved bill)"
    pid = api.create(name="edit-bill-item", price=100, purchasePrice=60, stock=20)[2]["id"]
    _, _, sale = api.sale([{"productId": pid, "qty": 2}], amountPaid=200)
    sale_id = sale["id"]

    status, _, updated = api.json("POST", f"/api/sale/{sale_id}",
                                   {"items": [{"productId": pid, "qty": 1}], "amountPaid": 300})
    record(c, "Adding an item to a saved bill updates its totals and merges the rows",
           status == 200 and updated["subtotal"] == 300 and updated["total"] == 300 and len(updated["items"]) == 2,
           f"status={status} sale={updated}")
    stock = find(api, "edit-bill-item")["stock"]
    record(c, "Stock is reduced only for the newly added quantity, not the original items again",
           stock == 17, f"stock={stock}")

    status, _, updated = api.json("POST", f"/api/sale/{sale_id}",
                                   {"items": [], "discount": 30, "amountPaid": 270})
    record(c, "Editing discount with no new items updates the total without duplicating rows",
           status == 200 and updated["discount"] == 30 and updated["total"] == 270 and len(updated["items"]) == 2,
           f"status={status} sale={updated}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}",
                             {"items": [{"productId": pid, "qty": 10000}], "amountPaid": 10 ** 9})
    stock_after = find(api, "edit-bill-item")["stock"]
    record(c, "Adding more items than are in stock is rejected and stock is untouched",
           status >= 400 and stock_after == 17, f"status={status} stock={stock_after}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}",
                             {"items": [{"productId": pid, "qty": 1}], "amountPaid": 1})
    stock_after = find(api, "edit-bill-item")["stock"]
    record(c, "Underpayment on a bill edit is rejected and stock is untouched",
           status >= 400 and stock_after == 17, f"status={status} stock={stock_after}")

    status, _, _ = api.json("POST", "/api/sale/does-not-exist",
                             {"items": [{"productId": pid, "qty": 1}], "amountPaid": 10 ** 9})
    record(c, "Editing a bill that does not exist returns 404", status == 404, f"status={status}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}",
                             {"items": [{"productId": "no-such-product", "qty": 1}], "amountPaid": 10 ** 9})
    record(c, "Adding a non-existent product to a bill is rejected", status == 404, f"status={status}")

    status, _, _ = api.request("POST", f"/api/sale/{sale_id}", headers={"Origin": EVIL},
                                body={"items": [{"productId": pid, "qty": 1}], "amountPaid": 10 ** 9})
    record(c, "Cross-site POST to the bill-edit endpoint is rejected", status >= 400, f"status={status}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}",
                             {"items": [{"productId": pid, "qty": 1}], "amountPaid": 10 ** 9}, auth=False)
    record(c, "Bill-edit endpoint requires a login like every other route", status == 401, f"status={status}")


# ---------------------------------------------------------------- void / cancel a bill
def test_void_sale(api):
    c = "Voiding a bill"
    pid = api.create(name="void-item", price=50, purchasePrice=20, stock=30)[2]["id"]
    stock_before = find(api, "void-item")["stock"]
    _, _, sale = api.sale([{"productId": pid, "qty": 4}], amountPaid=200)
    sale_id = sale["id"]
    stock_after_sale = find(api, "void-item")["stock"]
    record(c, "Sanity: selling reduces stock", stock_after_sale == stock_before - 4, f"stock={stock_after_sale}")

    status, _, _ = api.request("POST", f"/api/sale/{sale_id}/void", headers={"Origin": EVIL})
    record(c, "Cross-site POST to the void endpoint is rejected", status >= 400, f"status={status}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}/void", {"reason": "test"}, auth=False)
    record(c, "Voiding requires a login like every other route", status == 401, f"status={status}")

    status, _, voided = api.json("POST", f"/api/sale/{sale_id}/void", {"reason": "customer returned everything"})
    stock_restored = find(api, "void-item")["stock"]
    record(c, "Voiding a bill marks it voided and restores stock for every line",
           status == 200 and voided.get("voided") is True and stock_restored == stock_before,
           f"status={status} voided={voided and voided.get('voided')} stock={stock_restored}")

    _, _, sales = api.json("GET", "/api/sales")
    kept = next((s for s in sales or [] if s.get("id") == sale_id), None)
    record(c, "The voided bill stays in sales history (not deleted) with its original total",
           kept is not None and kept.get("voided") is True and kept.get("total") == sale["total"],
           f"kept={kept}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}/void", {})
    record(c, "Voiding an already-voided bill is rejected", status == 409, f"status={status}")

    status, _, _ = api.json("POST", f"/api/sale/{sale_id}",
                             {"items": [{"productId": pid, "qty": 1}], "amountPaid": 10 ** 9})
    record(c, "A voided bill can no longer be edited (add items)", status == 409, f"status={status}")

    status, _, _ = api.json("POST", "/api/sale/does-not-exist/void", {})
    record(c, "Voiding a bill that does not exist returns 404", status == 404, f"status={status}")

    # Product deleted after the sale: neither its id nor its name exist anymore, so void
    # cannot restock that line - it must still succeed overall and report the mismatch
    # instead of silently losing track of the stock discrepancy.
    pid2 = api.create(name="void-delete-item", price=10, purchasePrice=5, stock=10)[2]["id"]
    _, _, sale2 = api.sale([{"productId": pid2, "qty": 2}], amountPaid=100)
    api.request("DELETE", f"/api/products/{pid2}")
    status, _, voided2 = api.json("POST", f"/api/sale/{sale2['id']}/void", {})
    record(c, "Voiding a bill whose product was later deleted still succeeds and reports the mismatch",
           status == 200 and voided2.get("voided") is True and bool(voided2.get("restockWarnings")),
           f"status={status} voided={voided2}")


# ---------------------------------------------------------------- credit customers ("khata")
def test_credit_customers(api):
    c = "Credit customers"

    status, _, _ = api.json("GET", "/api/customers", auth=False)
    record(c, "Customer list requires a login", status == 401, f"status={status}")

    status, _, _ = api.request("POST", "/api/customers", headers={"Origin": EVIL},
                                body={"name": "csrf-customer", "phone": ""})
    record(c, "Cross-site POST to create a customer is rejected", status >= 400, f"status={status}")

    status, _, cust = api.customer(name="Credit Test Customer", phone="0300-1234567")
    ok = status == 201 and isinstance(cust, dict) and cust.get("balance") == 0
    record(c, "Creating a customer succeeds with a zero starting balance", ok, f"status={status} cust={cust}")
    if not ok:
        raise RuntimeError("could not create test customer, skipping the rest of this group")
    customer_id = cust["id"]

    status, _, _ = api.customer(name="Credit Test Customer", phone="other")
    record(c, "A second customer with the same name is rejected", status == 409, f"status={status}")

    pid = api.create(name="credit-item", price=200, purchasePrice=120, stock=50)[2]["id"]

    status, _, _ = api.sale([{"productId": pid, "qty": 1}], paymentMethod="Credit", amountPaid=0)
    record(c, "A Credit sale with no customer attached is rejected",
           status == 400, f"status={status}")

    status, _, _ = api.sale([{"productId": pid, "qty": 1}], paymentMethod="Credit",
                             amountPaid=0, customerId="no-such-customer")
    record(c, "A sale billed to a non-existent customer is rejected", status == 404, f"status={status}")

    status, _, credit_sale = api.sale([{"productId": pid, "qty": 2}], paymentMethod="Credit",
                                       amountPaid=100, customerId=customer_id,
                                       customerName="ignored", customerPhone="ignored")
    ok = (status == 200 and credit_sale.get("customerId") == customer_id
          and credit_sale.get("customerName") == "Credit Test Customer" and credit_sale.get("total") == 400)
    record(c, "A credit sale can be only partly paid, and the customer record (not client text) is canonical",
           ok, f"status={status} sale={credit_sale}")

    status, _, custs = api.json("GET", "/api/customers")
    found = next((item for item in custs or [] if item["id"] == customer_id), None)
    record(c, "The customer's balance reflects the unpaid part of the credit sale (400 - 100 = 300)",
           found is not None and found["balance"] == 300, f"customer={found}")

    status, _, _ = api.sale([{"productId": pid, "qty": 1}], paymentMethod="Cash",
                             amountPaid=50, customerId=customer_id)
    record(c, "A partial payment is also allowed for any sale linked to a credit customer, "
              "even without selecting Credit as the payment method",
           status == 200, f"status={status}")

    status, _, _ = api.request("POST", f"/api/customers/{customer_id}/payment", headers={"Origin": EVIL},
                                body={"amount": 50, "note": ""})
    record(c, "Cross-site POST to record a payment is rejected", status >= 400, f"status={status}")

    status, _, _ = api.json("POST", f"/api/customers/{customer_id}/payment", {"amount": -10, "note": ""})
    record(c, "A non-positive payment amount is rejected", status == 400, f"status={status}")

    status, _, _ = api.json("POST", "/api/customers/no-such-customer/payment", {"amount": 10, "note": ""})
    record(c, "Recording a payment for a non-existent customer is rejected", status == 404, f"status={status}")

    status, _, payment = api.json("POST", f"/api/customers/{customer_id}/payment", {"amount": 200, "note": "cash drop"})
    record(c, "Recording a payment succeeds", status == 200 and payment.get("amount") == 200, f"status={status}")

    _, _, custs = api.json("GET", "/api/customers")
    found = next((item for item in custs or [] if item["id"] == customer_id), None)
    # 300 (first sale) + 150 (second sale: 200 - 50 paid) - 200 (payment) = 250
    record(c, "Balance after the payment reflects every credit sale and payment so far",
           found is not None and found["balance"] == 250, f"customer={found}")

    status, _, updated = api.json("PUT", f"/api/customers/{customer_id}",
                                   {"name": "Renamed Credit Customer", "phone": "0300-7654321"})
    record(c, "Updating a customer's name/phone succeeds", status == 200 and updated.get("name") == "Renamed Credit Customer",
           f"status={status} updated={updated}")

    status, _, _ = api.json("PUT", "/api/customers/no-such-customer", {"name": "X", "phone": ""})
    record(c, "Updating a non-existent customer returns 404", status == 404, f"status={status}")

    status, _, voided = api.json("POST", f"/api/sale/{credit_sale['id']}/void", {})
    record(c, "Voiding a credit sale succeeds", status == 200 and voided.get("voided") is True, f"status={status}")

    _, _, custs = api.json("GET", "/api/customers")
    found = next((item for item in custs or [] if item["id"] == customer_id), None)
    # 250 - 300 (the voided sale's due no longer counts) = -50 (customer is now in credit)
    record(c, "Voiding a credit sale removes its due amount from the customer's balance",
           found is not None and found["balance"] == -50, f"customer={found}")


def test_race(api):
    c = "Concurrency"
    pid = api.create(name="race", price=10, stock=5)[2]["id"]
    statuses = []

    def buy():
        try:
            statuses.append(api.sale([{"productId": pid, "qty": 1}])[0])
        except OSError as e:
            statuses.append(str(e))

    threads = [threading.Thread(target=buy) for _ in range(25)]
    [t.start() for t in threads]
    [t.join() for t in threads]
    stock = find(api, "race")["stock"]
    record(c, "25 simultaneous sales of 5 items never oversell", statuses.count(200) == 5 and stock == 0,
           f"successes={statuses.count(200)}, stock={stock}")

    _, _, sales = api.json("GET", "/api/sales")
    nums = [s["billNo"] for s in sales or []]
    record(c, "Bill numbers stay unique under concurrency", sales is not None and len(nums) == len(set(nums)), "history unreadable" if sales is None else f"dupes in {nums}")


# ---------------------------------------------------------------- DoS / robustness
def test_dos(api):
    c = "DoS / robustness"
    try:
        status, _, _ = api.request("POST", "/api/products", raw_body=b"x" * (3 * 1024 * 1024),
                                   headers={"Content-Type": "application/json"}, timeout=10)
    except OSError as e:
        status = str(e)
    record(c, "3 MB body rejected", status == 413 or api.alive(), f"status {status}")
    record(c, "Server alive after oversized body", api.alive())

    host = api.host.encode()
    # These are hand-crafted raw requests, not Api.request(), so they don't get the
    # auto-attached Authorization header - add it here for the ones that target a
    # protected route, otherwise they'd just bounce off the auth check (401) before
    # ever reaching the parsing code these payloads are meant to stress-test.
    auth_hdr = f"Authorization: Bearer {api.token}\r\n".encode()
    for label, payload in [
        ("garbage bytes", os.urandom(2048)),
        ("no request line", b"\r\n\r\n"),
        ("negative Content-Length", b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: -50\r\n\r\n{}"),
        ("non-numeric Content-Length", b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: abc\r\n\r\n{}"),
        ("invalid JSON", b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: 9\r\n\r\n{\"name\":["),
        ("deeply nested JSON", b"POST /api/sale HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: 20000\r\n\r\n" + b"[" * 10000 + b"]" * 10000),
        ("huge header", b"GET /api/shop HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"X-A: " + b"a" * 900_000 + b"\r\n\r\n"),
        ("invalid UTF-8", b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: 40\r\n\r\n{\"name\":\"\xff\xfe\xc0\",\"price\":1,\"stock\":1}  "),
        ("truncated \\u escape", b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: 18\r\n\r\n{\"name\":\"ab\\u12\"}"),
    ]:
        try:
            api.raw(payload, timeout=8)
        except OSError:
            pass
        record(c, f"Survives {label}", api.alive())

    # Content-Length says 1000 bytes but only 10 arrive: one stuck client must not block others.
    s = socket.create_connection(("127.0.0.1", api.port))
    s.sendall(b"POST /api/products HTTP/1.1\r\nHost: " + host + b"\r\n" + auth_hdr + b"Content-Length: 1000\r\n\r\n0123456789")
    t = time.time()
    ok = api.alive()
    record(c, "Stalled request body does not block other users", ok and time.time() - t < 2)
    s.close()

    # Slowloris: many connections trickling headers forever.
    socks = []
    for _ in range(300):
        try:
            k = socket.create_connection(("127.0.0.1", api.port), timeout=2)
            k.sendall(b"GET /api/shop HTTP/1.1\r\nHost: " + host + b"\r\n")
            socks.append(k)
        except OSError:
            break
    stop = threading.Event()

    def trickle():
        while not stop.is_set():
            for k in list(socks):
                try:
                    k.sendall(b"X: y\r\n")
                except OSError:
                    socks.remove(k)
            time.sleep(1)

    tr = threading.Thread(target=trickle, daemon=True)
    tr.start()
    time.sleep(2)
    t = time.time()
    ok = api.alive()
    record(c, f"Serves real users during slowloris ({len(socks)} slow connections)", ok, f"took {time.time() - t:.1f}s")
    time.sleep(14)
    remaining = len([k for k in socks if k.fileno() != -1])
    survivors = 0
    for k in socks:
        try:
            k.sendall(b"X: y\r\n")
            k.settimeout(0.2)
            if k.recv(1) == b"":
                continue
            survivors += 1
        except (socket.timeout, TimeoutError):
            survivors += 1  # still open, still waiting for us
        except OSError:
            pass
    stop.set()
    for k in socks:
        k.close()
    record(c, "Slow connections are cut off after a deadline", survivors == 0, f"{survivors}/{remaining} still open after 16s")

    before = len(os.listdir(os.path.join(api.workdir, "backups"))) if os.path.isdir(os.path.join(api.workdir, "backups")) else 0
    os.makedirs(os.path.join(api.workdir, "backups"), exist_ok=True)
    for i in range(300):  # simulate months of saved backups
        open(os.path.join(api.workdir, "backups", f"products.txt.2020-01-01_00-00-{i:03d}.bak"), "w").close()
    for _ in range(5):
        api.request("POST", "/api/backup")
    after = len(os.listdir(os.path.join(api.workdir, "backups")))
    record(c, "Backup folder cannot grow without limit", after <= 150, f"{before} -> {after} files")
    record(c, "Server alive at end of DoS tests", api.alive())


# ---------------------------------------------------------------- headers
def test_headers(api):
    c = "Security headers"
    _, h, _ = api.request("GET", "/api/products")
    record(c, "X-Content-Type-Options: nosniff", h.get("x-content-type-options") == "nosniff")
    record(c, "Clickjacking protection (X-Frame-Options / frame-ancestors)",
           "x-frame-options" in h or "frame-ancestors" in h.get("content-security-policy", ""))
    record(c, "API responses not cached (Cache-Control: no-store)", "no-store" in h.get("cache-control", ""))
    lan = None
    try:
        lan = socket.gethostbyname(socket.gethostname())
    except OSError:
        pass
    if lan and not lan.startswith("127."):
        try:
            socket.create_connection((lan, api.port), timeout=1).close()
            exposed = True
        except OSError:
            exposed = False
        record(c, f"Not reachable from the network ({lan})", not exposed)


# ---------------------------------------------------------------- frontend static checks
def test_frontend():
    c = "Frontend"
    js = open(os.path.join(ROOT, "script.js"), encoding="utf-8").read()
    m = re.search(r"const apiHost = (.+);", js)
    unrestricted = m and "configuredServer ||" in m.group(1)
    record(c, "?server= link cannot send shop data to an arbitrary host", not unrestricted)
    csv_fn = re.search(r"const escapeCsv = ([\s\S]{0,400})", js)
    record(c, "CSV export neutralises spreadsheet formulas (=, +, -, @)", bool(csv_fn and "[=+" in csv_fn.group(1)))
    unsafe = []
    for m in re.finditer(r"\$\{([^}]*)\}", js):
        expr = m.group(1)
        if re.search(r"\.(name|category|unit|customerName|customerPhone|shopName|shopAddress|shopPhone|paymentMethod|createdAt|id)\b", expr) \
                and "escapeHtml" not in expr and "formatAmount" not in expr and "API_BASE" not in expr:
            line = js.count("\n", 0, m.start()) + 1
            ctx = js[max(0, m.start() - 200):m.start()]
            if "innerHTML" in ctx or "<" in ctx[-80:]:
                unsafe.append(f"line {line}: {expr}")
    record(c, "Every user-controlled value put into HTML is escaped", not unsafe, "; ".join(unsafe[:5]))


# ---------------------------------------------------------------- harness
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.join(ROOT, "backend.exe"))
    ap.add_argument("--port", type=int, default=18080)
    args = ap.parse_args()

    workdir = tempfile.mkdtemp(prefix="shop_sectest_")
    exe = os.path.join(workdir, "backend.exe")
    shutil.copy(args.exe, exe)
    for f in ["index.html", "script.js", "styles.css"]:
        shutil.copy(os.path.join(ROOT, f), workdir)
    if os.path.isdir(os.path.join(ROOT, "themes")):
        shutil.copytree(os.path.join(ROOT, "themes"), os.path.join(workdir, "themes"))
    proc = subprocess.Popen([exe, str(args.port)], cwd=workdir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    api = Api(args.port)
    api.workdir = workdir
    for _ in range(50):
        if api.alive():
            break
        time.sleep(0.1)
    else:
        print("Backend did not start")
        proc.kill()
        sys.exit(2)

    try:
        try:
            test_auth(api)
        except Exception as e:
            record(test_auth.__name__, "test group crashed", False, repr(e))

        if not api.token:
            print("Could not authenticate against the backend; skipping the rest of the suite.")
        else:
            for test in (test_access_control, test_injection, test_business_logic, test_bill_edit,
                         test_void_sale, test_credit_customers, test_race, test_dos, test_headers):
                try:
                    test(api)
                except Exception as e:  # a crash inside a test group is itself a finding
                    record(test.__name__, "test group crashed", False, repr(e))
            test_frontend()
        record("Stability", "Backend process still running at the end", proc.poll() is None)
    finally:
        proc.kill()
        proc.wait()
        shutil.rmtree(workdir, ignore_errors=True)

    failed = [r for r in results if not r[2]]
    print(f"\n{len(results) - len(failed)}/{len(results)} passed")
    with open(os.path.join(ROOT, "tests", "last_results.json"), "w") as f:
        json.dump([{"category": a, "test": b, "pass": ok, "detail": d} for a, b, ok, d in results], f, indent=1)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
