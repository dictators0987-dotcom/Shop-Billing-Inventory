const STORAGE_KEYS = {
  products: 'shop_inventory_products_v1',
  shopName: 'shop_inventory_shop_name_v1',
  shopPhone: 'shop_inventory_shop_phone_v1',
  shopAddress: 'shop_inventory_shop_address_v1',
  sales: 'shop_inventory_sales_v1',
  businessDayStartedAt: 'shop_inventory_business_day_started_at_v1',
  monthlyProfitStartedAt: 'shop_inventory_monthly_profit_started_at_v1',
  printerSize: 'shop_inventory_printer_size_v1',
  design: 'shop_inventory_design_v1',
  activeTab: 'shop_inventory_active_tab_v1',
};

// The backend only listens on this computer, so the API is always on localhost.
// A ?server= value is accepted only if it names this computer; anything else could
// send the shop's data to another host through a crafted link.
const BACKEND_PORT = '8080';
const LOCAL_HOSTS = ['localhost', '127.0.0.1'];
const configuredServer = new URLSearchParams(window.location.search).get('server');
const apiHost = LOCAL_HOSTS.includes(configuredServer) ? configuredServer
  : LOCAL_HOSTS.includes(window.location.hostname) ? window.location.hostname : 'localhost';
const API_BASE = `http://${apiHost}:${BACKEND_PORT}/api`;

// Kept in sessionStorage (not localStorage) so closing the tab/browser forgets it and the
// login screen reappears the next time the software is opened, while a page refresh inside
// the same tab does not pointlessly ask again.
const AUTH_TOKEN_KEY = 'shop_inventory_auth_token_v1';
let authToken = null;

function getAuthToken() {
  if (authToken) return authToken;
  try {
    authToken = sessionStorage.getItem(AUTH_TOKEN_KEY);
  } catch (error) {
    authToken = null;
  }
  return authToken;
}

function setAuthToken(token) {
  authToken = token;
  try {
    if (token) sessionStorage.setItem(AUTH_TOKEN_KEY, token);
    else sessionStorage.removeItem(AUTH_TOKEN_KEY);
  } catch (error) {
    // The token still applies for this visit even if it cannot be remembered.
  }
}

const state = {
  products: [],
  bill: [],
  sales: [],
  editingProductId: null,
  editingSaleId: null,
  shopName: 'MY SHOP NAME',
  shopPhone: '',
  shopAddress: '',
  lastReceipt: null,
  businessDayStartedAt: 0,
  monthlyProfitStartedAt: 0,
  printerSize: '80',
  customers: [],
};

const inventoryTableBody = document.getElementById('inventoryTableBody');
const searchResults = document.getElementById('searchResults');
const productForm = document.getElementById('productForm');
const productNameInput = document.getElementById('productNameInput');
const productPriceInput = document.getElementById('productPriceInput');
const productPurchasePriceInput = document.getElementById('productPurchasePriceInput');
const productStockInput = document.getElementById('productStockInput');
const formMessage = document.getElementById('formMessage');
const cancelEditBtn = document.getElementById('cancelEditBtn');
const saveProductBtn = document.getElementById('saveProductBtn');
const quickScanInput = document.getElementById('quickScanInput');
const searchInput = document.getElementById('searchInput');
const billItemsList = document.getElementById('billItemsList');
const billGrandTotal = document.getElementById('billGrandTotal');
const generateBillBtn = document.getElementById('generateBillBtn');
const newBillBtn = document.getElementById('newBillBtn');
const startNewDayBtn = document.getElementById('startNewDayBtn');
const backupNowBtn = document.getElementById('backupNowBtn');
const dayResetMessage = document.getElementById('dayResetMessage');
const paymentMethodInput = document.getElementById('paymentMethodInput');
const discountInput = document.getElementById('discountInput');
const taxInput = document.getElementById('taxInput');
const amountPaidInput = document.getElementById('amountPaidInput');
const customerNameInput = document.getElementById('customerNameInput');
const customerPhoneInput = document.getElementById('customerPhoneInput');
const amountDueValue = document.getElementById('amountDueValue');
const changeValue = document.getElementById('changeValue');
const billEditBanner = document.getElementById('billEditBanner');
const billEditBannerText = document.getElementById('billEditBannerText');
const shopNameInput = document.getElementById('shopNameInput');
const shopPhoneInput = document.getElementById('shopPhoneInput');
const shopAddressInput = document.getElementById('shopAddressInput');
const printerSizeInput = document.getElementById('printerSizeInput');
const designInput = document.getElementById('designInput');
const languageInput = document.getElementById('languageInput');
const currencyInput = document.getElementById('currencyInput');
const trendBadge = document.getElementById('trendBadge');
const trendDetail = document.getElementById('trendDetail');
const salesChart = document.getElementById('salesChart');
const periodButtons = document.querySelectorAll('.period-btn');
const logoutBtn = document.getElementById('logoutBtn');
const changePasswordForm = document.getElementById('changePasswordForm');
const currentPasswordInput = document.getElementById('currentPasswordInput');
const newPasswordInput = document.getElementById('newPasswordInput');
const confirmNewPasswordInput = document.getElementById('confirmNewPasswordInput');
const changePasswordMessage = document.getElementById('changePasswordMessage');
const changePasswordBtn = document.getElementById('changePasswordBtn');
const recoveryQuestionForm = document.getElementById('recoveryQuestionForm');
const currentRecoveryQuestionText = document.getElementById('currentRecoveryQuestionText');
const recoveryCurrentPasswordInput = document.getElementById('recoveryCurrentPasswordInput');
const recoveryQuestionInput = document.getElementById('recoveryQuestionInput');
const recoveryAnswerInput = document.getElementById('recoveryAnswerInput');
const recoveryQuestionMessage = document.getElementById('recoveryQuestionMessage');
const saveRecoveryQuestionBtn = document.getElementById('saveRecoveryQuestionBtn');
const appShell = document.getElementById('appShell');
const authOverlay = document.getElementById('authOverlay');
const authForm = document.getElementById('authForm');
const authHeading = document.getElementById('authHeading');
const authSubtitle = document.getElementById('authSubtitle');
const authUsernameGroup = document.getElementById('authUsernameGroup');
const authUsernameInput = document.getElementById('authUsernameInput');
const authSecurityQuestionSetGroup = document.getElementById('authSecurityQuestionSetGroup');
const authSecurityQuestionInput = document.getElementById('authSecurityQuestionInput');
const authRecoveryQuestionText = document.getElementById('authRecoveryQuestionText');
const authSecurityAnswerGroup = document.getElementById('authSecurityAnswerGroup');
const authSecurityAnswerLabel = document.getElementById('authSecurityAnswerLabel');
const authSecurityAnswerInput = document.getElementById('authSecurityAnswerInput');
const authPasswordLabel = document.getElementById('authPasswordLabel');
const authPasswordInput = document.getElementById('authPasswordInput');
const authConfirmGroup = document.getElementById('authConfirmGroup');
const authConfirmPasswordLabel = document.getElementById('authConfirmPasswordLabel');
const authConfirmPasswordInput = document.getElementById('authConfirmPasswordInput');
const authForgotPasswordBtn = document.getElementById('authForgotPasswordBtn');
const authBackToLoginBtn = document.getElementById('authBackToLoginBtn');
const authSubmitBtn = document.getElementById('authSubmitBtn');
const authFormMessage = document.getElementById('authFormMessage');
const designStylesheet = document.getElementById('designStylesheet');
const shopDisplayName = document.getElementById('shopDisplayName');
const shopPhoneDisplay = document.getElementById('shopPhoneDisplay');
const shopAddressDisplay = document.getElementById('shopAddressDisplay');
const currentDateDisplay = document.getElementById('currentDateDisplay');
const shopSettingsBtn = document.getElementById('shopSettingsBtn');
const shopSettingsPanel = document.getElementById('shopSettingsPanel');
const receiptContainer = document.getElementById('receiptContainer');
const salesHistoryList = document.getElementById('salesHistoryList');
const addCustomerForm = document.getElementById('addCustomerForm');
const newCustomerNameInput = document.getElementById('newCustomerNameInput');
const newCustomerPhoneInput = document.getElementById('newCustomerPhoneInput');
const customerFormMessage = document.getElementById('customerFormMessage');
const customersList = document.getElementById('customersList');
const creditCustomerGroup = document.getElementById('creditCustomerGroup');
const creditCustomerInput = document.getElementById('creditCustomerInput');
const customerNameGroup = document.getElementById('customerNameGroup');
const customerPhoneGroup = document.getElementById('customerPhoneGroup');
const amountPaidLabel = document.getElementById('amountPaidLabel');
const changeLabel = document.getElementById('changeLabel');
const creditOutstandingStat = document.getElementById('creditOutstandingStat');
const totalProductsStat = document.getElementById('totalProductsStat');
const lowStockStat = document.getElementById('lowStockStat');
const todaySalesStat = document.getElementById('todaySalesStat');
const totalRevenueStat = document.getElementById('totalRevenueStat');
const todayProfitStat = document.getElementById('todayProfitStat');
const monthlyProfitStat = document.getElementById('monthlyProfitStat');
const resetMonthlyProfitBtn = document.getElementById('resetMonthlyProfitBtn');
const todayBillsStat = document.getElementById('todayBillsStat');
const bestSellersList = document.getElementById('bestSellersList');
const recentTransactionsList = document.getElementById('recentTransactionsList');
const lowStockProductsList = document.getElementById('lowStockProductsList');
const outOfStockProductsList = document.getElementById('outOfStockProductsList');
const connectionStatus = document.getElementById('connectionStatus');
const productCategoryInput = document.getElementById('productCategoryInput');
const productUnitInput = document.getElementById('productUnitInput');
const productThresholdInput = document.getElementById('productThresholdInput');
const categoryFilterInput = document.getElementById('categoryFilterInput');
const stockFilterInput = document.getElementById('stockFilterInput');
const inventorySortInput = document.getElementById('inventorySortInput');
const appLayout = document.getElementById('appLayout');
const tabButtons = document.querySelectorAll('.tab-btn');
const billTabCount = document.getElementById('billTabCount');
const productFormTitle = document.getElementById('productFormTitle');
const TAB_NAMES = ['dashboard', 'bill', 'inventory', 'add-product', 'history', 'customers', 'analysis'];

const DEFAULT_CATEGORIES = ['Hardware', 'Paints', 'Cenentry'];
let shopSyncTimer = null;
let billInProgress = false;
let analysisPeriod = 'monthly';

languageInput.value = getLanguage();
currencyInput.value = getCurrency();

// Product names such as 4 " Elbow must not break the HTML they are inserted into.
function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

// Alternative designs are extra stylesheets layered over styles.css.
const DESIGNS = { classic: null, counter: 'themes/counter.css', fandeck: 'themes/fandeck.css' };

function readStoredDesign() {
  try {
    return localStorage.getItem(STORAGE_KEYS.design);
  } catch (error) {
    return null;
  }
}

function applyDesign(name) {
  const design = Object.hasOwn(DESIGNS, name) ? name : 'classic';
  const href = DESIGNS[design];
  if (href) {
    designStylesheet.setAttribute('href', href);
    designStylesheet.disabled = false;
  } else {
    designStylesheet.disabled = true;
    designStylesheet.removeAttribute('href');
  }
  document.documentElement.dataset.design = design;
  designInput.value = design;
  return design;
}

applyDesign(new URLSearchParams(window.location.search).get('design') || readStoredDesign());

function roundMoney(value) {
  return Math.round((Number(value) || 0) * 100) / 100;
}

function formatAmount(value) {
  return Number(value || 0).toLocaleString('en-PK', { minimumFractionDigits: 0, maximumFractionDigits: 2 });
}

function formatCurrency(value) {
  return `${getCurrencySymbol()} ${formatAmount(value)}`;
}

function setFormMessage(message, type = '') {
  formMessage.textContent = message;
  formMessage.className = 'form-message';
  if (type) {
    formMessage.classList.add(type);
  }
}

// translateStaticUI() (run on every language/currency switch) only knows about
// [data-i18n] elements with fixed text, so this badge is kept out of that sweep and
// re-rendered from the last known status instead - otherwise a switch mid-session would
// stomp "Backend online" back to "Connecting..." even though nothing about the
// connection changed.
let lastConnectionStatus = 'checking';

function setConnectionStatus(status) {
  lastConnectionStatus = status;
  connectionStatus.className = `connection-status ${status}`;
  connectionStatus.textContent = status === 'online' ? t('backendOnline') : status === 'offline' ? t('backendOffline') : t('connecting');
}

function updateShopDisplay() {
  shopDisplayName.textContent = state.shopName.toUpperCase();
  shopPhoneDisplay.textContent = state.shopPhone ? t('phonePrefixed', { phone: state.shopPhone }) : '';
  shopAddressDisplay.textContent = state.shopAddress ? state.shopAddress : '';
  shopPhoneDisplay.classList.toggle('hidden', !state.shopPhone);
  shopAddressDisplay.classList.toggle('hidden', !state.shopAddress);
  currentDateDisplay.textContent = new Intl.DateTimeFormat(getDateLocale(), {
    weekday: 'long',
    month: 'long',
    day: 'numeric',
    year: 'numeric',
  }).format(new Date());
}

// Used only by the auth screen itself (status/setup/login/logout), before a session
// token exists. It never attaches a token and never triggers the "session expired"
// overlay - that dance belongs to apiFetch, which every other call goes through.
async function authApiFetch(path, options = {}) {
  let response;

  try {
    response = await fetch(`${API_BASE}${path}`, {
      headers: {
        'Content-Type': 'application/json',
      },
      ...options,
    });
  } catch (error) {
    setConnectionStatus('offline');
    throw new Error(window.location.protocol === 'file:'
      ? t('openAppInstead')
      : t('cannotConnectBackend'));
  }

  setConnectionStatus('online');

  const text = await response.text();
  let data = {};

  try {
    data = text ? JSON.parse(text) : {};
  } catch (error) {
    throw new Error(t('invalidBackendResponse'));
  }

  if (!response.ok) {
    const error = new Error(data.error || t('requestFailed', { status: response.status }));
    error.status = response.status;
    throw error;
  }

  return data;
}

async function apiFetch(path, options = {}) {
  const headers = { 'Content-Type': 'application/json', ...(options.headers || {}) };
  const token = getAuthToken();
  if (token) headers.Authorization = `Bearer ${token}`;

  let response;
  try {
    response = await fetch(`${API_BASE}${path}`, { ...options, headers });
  } catch (error) {
    setConnectionStatus('offline');
    throw new Error(window.location.protocol === 'file:'
      ? t('openAppInstead')
      : t('cannotConnectBackend'));
  }

  setConnectionStatus('online');

  // The backend rejects every /api/* call without a valid session token, so this is
  // the one place a lost/expired session is detected - send the shop owner back to
  // the login screen instead of leaving a broken page behind.
  if (response.status === 401) {
    setAuthToken(null);
    showAuthOverlay('login', t('authSessionExpired'));
  }

  const text = await response.text();
  let data = {};

  try {
    data = text ? JSON.parse(text) : {};
  } catch (error) {
    throw new Error(t('invalidBackendResponse'));
  }

  if (!response.ok) {
    throw new Error(data.error || t('requestFailed', { status: response.status }));
  }

  return data;
}

function persistProductsLocal() {
  localStorage.setItem(STORAGE_KEYS.products, JSON.stringify(state.products));
}

function persistShopNameLocal() {
  localStorage.setItem(STORAGE_KEYS.shopName, state.shopName);
  localStorage.setItem(STORAGE_KEYS.shopPhone, state.shopPhone);
  localStorage.setItem(STORAGE_KEYS.shopAddress, state.shopAddress);
}

function persistSalesLocal() {
  localStorage.setItem(STORAGE_KEYS.sales, JSON.stringify(state.sales));
}

function normalizeProduct(product) {
  return {
    ...product,
    purchasePrice: Number.isFinite(Number(product.purchasePrice)) ? Number(product.purchasePrice) : 0,
    category: product.category || 'General',
    unit: product.unit || 'piece',
    lowStockThreshold: Number.isInteger(Number(product.lowStockThreshold)) ? Number(product.lowStockThreshold) : 5,
  };
}

async function readProductsFromStorage() {
  try {
    const products = await apiFetch('/products');
    state.products = Array.isArray(products) ? products.map(normalizeProduct) : [];
    persistProductsLocal();
    return;
  } catch (error) {
    const saved = localStorage.getItem(STORAGE_KEYS.products);
    if (!saved) {
      state.products = [];
      persistProductsLocal();
      return;
    }

    try {
      const parsed = JSON.parse(saved);
      state.products = Array.isArray(parsed) ? parsed.map(normalizeProduct) : [];
    } catch (loadError) {
      state.products = [];
    }
  }
}

async function loadShopName() {
  try {
    const data = await apiFetch('/shop');
    state.shopName = data.shopName || 'MY SHOP NAME';
    state.shopPhone = data.shopPhone || '';
    state.shopAddress = data.shopAddress || '';
  } catch (error) {
    const stored = localStorage.getItem(STORAGE_KEYS.shopName);
    state.shopName = stored && stored.trim() ? stored.trim() : 'MY SHOP NAME';
    state.shopPhone = localStorage.getItem(STORAGE_KEYS.shopPhone) || '';
    state.shopAddress = localStorage.getItem(STORAGE_KEYS.shopAddress) || '';
  }

  shopNameInput.value = state.shopName;
  shopPhoneInput.value = state.shopPhone;
  shopAddressInput.value = state.shopAddress;
  state.printerSize = localStorage.getItem(STORAGE_KEYS.printerSize) || '80';
  printerSizeInput.value = state.printerSize;
  updateShopDisplay();
  persistShopNameLocal();
}

// Bills saved before numbering existed have no billNo. Bills are stored oldest first and the
// backend numbers new bills by their position, so an old bill's number is its position too.
function numberSales(salesOldestFirst) {
  salesOldestFirst.forEach((sale, index) => {
    if (!sale.billNo) sale.billNo = index + 1;
  });
  return salesOldestFirst;
}

async function loadSales() {
  try {
    const sales = await apiFetch('/sales');
    state.sales = Array.isArray(sales) ? numberSales(sales).reverse() : [];
    persistSalesLocal();
  } catch (error) {
    try {
      const saved = JSON.parse(localStorage.getItem(STORAGE_KEYS.sales) || '[]');
      state.sales = Array.isArray(saved) ? numberSales(saved.reverse()).reverse() : [];
    } catch (loadError) {
      state.sales = [];
    }
  }

  renderSalesHistory();
}

function persistShopName() {
  state.shopName = shopNameInput.value.trim() || 'MY SHOP NAME';
  state.shopPhone = shopPhoneInput.value.trim();
  // The backend stores the address on one line, so line breaks become commas.
  state.shopAddress = shopAddressInput.value.trim().replace(/\s*\n\s*/g, ', ');
  updateShopDisplay();
  persistShopNameLocal();

  // Wait until typing pauses; otherwise every keystroke saves (and backs up) the shop file.
  window.clearTimeout(shopSyncTimer);
  shopSyncTimer = window.setTimeout(syncShopDetails, 600);
}

async function syncShopDetails() {
  try {
    await apiFetch('/shop', {
      method: 'PUT',
      body: JSON.stringify({ shopName: state.shopName, shopPhone: state.shopPhone, shopAddress: state.shopAddress }),
    });
  } catch (error) {
    console.warn('Shop name sync failed:', error);
  }
}

function getProductById(productId) {
  return state.products.find((product) => product.id === productId);
}

function getBillItem(productId) {
  return state.bill.find((item) => item.productId === productId);
}

function calculateBillTotal() {
  return state.bill.reduce((total, item) => {
    const product = getProductById(item.productId);
    if (!product) return total;
    return total + product.price * item.qty;
  }, 0);
}

// When editing a saved bill to add more items, the subtotal covers the original items plus
// the new ones (matching handleAddItemsToSale's math), but discount/tax still come from the
// on-screen inputs - prefilled from the original bill in startAddItemsToSale(), and editable
// so the cashier can change the discount or tax for the whole bill, not just the new rows.
// Payment method/customer stay locked to the original sale since the backend keeps those as-is.
function getPaymentDetails(subtotalOverride) {
  const editingSale = state.editingSaleId ? state.sales.find((sale) => sale.id === state.editingSaleId) : null;
  const additionsSubtotal = calculateBillTotal();
  const subtotal = roundMoney(subtotalOverride !== undefined
    ? subtotalOverride
    : editingSale
      ? (editingSale.subtotal || 0) + additionsSubtotal
      : additionsSubtotal);

  const discount = Math.min(roundMoney(Math.max(0, parseMoneyValue(discountInput.value) || 0)), subtotal);
  const taxRate = Math.max(0, parseMoneyValue(taxInput.value) || 0);
  const discountedSubtotal = subtotal - discount;
  const tax = roundMoney(discountedSubtotal * (taxRate / 100));
  const total = roundMoney(discountedSubtotal + tax);
  const amountPaid = roundMoney(Math.max(0, parseMoneyValue(amountPaidInput.value) || 0));
  const paymentMethod = editingSale ? (editingSale.paymentMethod || 'Cash') : paymentMethodInput.value;

  return {
    paymentMethod,
    // Credit sales are billed to a known customer record, not free-text - see creditCustomerInput.
    customerId: editingSale ? (editingSale.customerId || '') : (paymentMethod === 'Credit' ? creditCustomerInput.value : ''),
    customerName: editingSale ? (editingSale.customerName || '') : customerNameInput.value.trim(),
    customerPhone: editingSale ? (editingSale.customerPhone || '') : customerPhoneInput.value.trim(),
    subtotal,
    discount,
    taxRate,
    tax,
    total,
    amountPaid,
    // Not clamped to 0: a negative value means the customer still owes this much (shown
    // as "Amount Due" instead of "Change" - see renderPaymentSummary).
    change: roundMoney(amountPaid - total),
  };
}

function renderPaymentSummary() {
  const details = getPaymentDetails();
  billGrandTotal.textContent = formatCurrency(details.total);
  amountDueValue.textContent = formatCurrency(details.total);
  // A negative change means the customer still owes it (a credit sale paid less than in
  // full) - shown as "Still Due" instead of a negative "Change" amount.
  if (details.change < -0.001) {
    changeLabel.textContent = t('stillDueLabel');
    changeValue.textContent = formatCurrency(Math.abs(details.change));
  } else {
    changeLabel.textContent = t('changeLabel');
    changeValue.textContent = formatCurrency(details.change);
  }
}

function getLowStockCount() {
  return state.products.filter((product) => product.stock > 0 && product.stock <= product.lowStockThreshold).length;
}

function getSaleDate(sale) {
  return String(sale.createdAt || '').slice(0, 10);
}

function getSaleMonth(sale) {
  return getSaleDate(sale).slice(0, 7);
}

function getDateKey(date) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
}

function initializeBusinessDay() {
  const now = new Date();
  const storedStart = Number(localStorage.getItem(STORAGE_KEYS.businessDayStartedAt));
  const storedDate = Number.isFinite(storedStart) && storedStart > 0 ? new Date(storedStart) : null;

  if (!storedDate || getDateKey(storedDate) !== getDateKey(now)) {
    state.businessDayStartedAt = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
    localStorage.setItem(STORAGE_KEYS.businessDayStartedAt, String(state.businessDayStartedAt));
    return;
  }

  state.businessDayStartedAt = storedStart;
}

function initializeMonthlyProfit() {
  const now = new Date();
  const storedStart = Number(localStorage.getItem(STORAGE_KEYS.monthlyProfitStartedAt));
  const storedDate = Number.isFinite(storedStart) && storedStart > 0 ? new Date(storedStart) : null;
  const currentMonth = getDateKey(now).slice(0, 7);

  if (!storedDate || getDateKey(storedDate).slice(0, 7) !== currentMonth) {
    state.monthlyProfitStartedAt = new Date(now.getFullYear(), now.getMonth(), 1).getTime();
    localStorage.setItem(STORAGE_KEYS.monthlyProfitStartedAt, String(state.monthlyProfitStartedAt));
    return;
  }

  state.monthlyProfitStartedAt = storedStart;
}

function getSaleTimestamp(sale) {
  const timestamp = Date.parse(String(sale.createdAt || '').replace(' ', 'T'));
  return Number.isFinite(timestamp) ? timestamp : 0;
}

// A voided bill stays in state.sales (and sales.jsonl) for the books, but should not
// count toward revenue/profit/best-sellers/CSV totals - this is the one place that list
// is built, so every stat/export function below filters through it instead of state.sales.
function activeSales() {
  return state.sales.filter((sale) => !sale.voided);
}

function renderDashboardInsights() {
  const sales = activeSales();
  const today = getDateKey(new Date());
  const todaySales = sales.filter((sale) => (
    getSaleDate(sale) === today && getSaleTimestamp(sale) >= state.businessDayStartedAt
  ));
  const todayRevenue = todaySales.reduce((sum, sale) => sum + Number(sale.total || 0), 0);
  const totalRevenue = sales.reduce((sum, sale) => sum + Number(sale.total || 0), 0);
  const currentMonth = getDateKey(new Date()).slice(0, 7);
  const monthlySales = sales.filter((sale) => (
    getSaleMonth(sale) === currentMonth && getSaleTimestamp(sale) >= state.monthlyProfitStartedAt
  ));
  const todayProfit = todaySales.reduce((sum, sale) => sum + Number(sale.profit || 0), 0);
  const monthlyProfit = monthlySales.reduce((sum, sale) => sum + Number(sale.profit || 0), 0);
  const bestSellers = new Map();

  sales.forEach((sale) => {
    (sale.items || []).forEach((item) => {
      const current = bestSellers.get(item.name) || 0;
      bestSellers.set(item.name, current + Number(item.qty || 0));
    });
  });

  todaySalesStat.textContent = formatCurrency(todayRevenue);
  totalRevenueStat.textContent = formatCurrency(totalRevenue);
  todayProfitStat.textContent = formatCurrency(todayProfit);
  monthlyProfitStat.textContent = formatCurrency(monthlyProfit);
  todayBillsStat.textContent = String(todaySales.length);
  // Only what customers owe counts as "outstanding" - a customer in credit (negative
  // balance) is the shop's liability, not money still to collect, so it isn't netted in.
  const creditOutstanding = state.customers.reduce((sum, customer) => sum + Math.max(0, Number(customer.balance || 0)), 0);
  creditOutstandingStat.textContent = formatCurrency(creditOutstanding);

  const bestSellerRows = [...bestSellers.entries()]
    .sort((first, second) => second[1] - first[1])
    .slice(0, 5);
  bestSellersList.innerHTML = bestSellerRows.length
    ? bestSellerRows.map(([name, quantity]) => `<div class="insight-row"><span>${escapeHtml(name)}</span><strong>${escapeHtml(t('soldSuffix', { qty: quantity }))}</strong></div>`).join('')
    : `<div class="insight-empty">${escapeHtml(t('noSalesRecorded'))}</div>`;

  recentTransactionsList.innerHTML = sales.length
    ? sales.slice(0, 5).map((sale) => `
        <div class="insight-row">
          <span>${escapeHtml(t('billNumberPrefix', { num: sale.billNo }))} · ${escapeHtml(sale.createdAt || t('savedBill'))}<small>${escapeHtml(sale.paymentMethod || t('paymentCash'))}</small></span>
          <strong>${formatCurrency(sale.total || 0)}</strong>
        </div>
      `).join('')
    : `<div class="insight-empty">${escapeHtml(t('noRecentTransactions'))}</div>`;

  const lowStockProducts = state.products
    .filter((product) => product.stock > 0 && product.stock <= product.lowStockThreshold)
    .sort((first, second) => first.stock - second.stock)
    .slice(0, 5);
  lowStockProductsList.innerHTML = lowStockProducts.length
    ? lowStockProducts.map((product) => `
        <div class="insight-row low-stock-insight">
          <span>${escapeHtml(product.name)}<small>${escapeHtml(t('thresholdPrefix', { value: product.lowStockThreshold }))}</small></span>
          <strong>${escapeHtml(t('leftSuffix', { stock: product.stock }))}</strong>
        </div>
      `).join('')
    : `<div class="insight-empty">${escapeHtml(t('allStockHealthy'))}</div>`;

  const outOfStockProducts = state.products.filter((product) => product.stock === 0).slice(0, 5);
  outOfStockProductsList.innerHTML = outOfStockProducts.length
    ? outOfStockProducts.map((product) => `
        <div class="insight-row out-of-stock-insight">
          <span>${escapeHtml(product.name)}<small>${escapeHtml(product.category)}</small></span>
          <strong>${escapeHtml(t('restock'))}</strong>
        </div>
      `).join('')
    : `<div class="insight-empty">${escapeHtml(t('noOutOfStock'))}</div>`;
}

// Groups sales revenue into the last `limit` calendar months / years so the dashboard
// can show a simple trend instead of a wall of numbers.
function getMonthlyRevenueSeries(limit) {
  const totals = new Map();
  activeSales().forEach((sale) => {
    const key = getSaleMonth(sale);
    if (!key) return;
    totals.set(key, (totals.get(key) || 0) + Number(sale.total || 0));
  });

  return [...totals.keys()].sort().slice(-limit).map((key) => {
    const [year, month] = key.split('-').map(Number);
    const label = new Intl.DateTimeFormat(getDateLocale(), { month: 'short', year: '2-digit' }).format(new Date(year, month - 1, 1));
    return { key, label, value: roundMoney(totals.get(key)) };
  });
}

function getYearlyRevenueSeries(limit) {
  const totals = new Map();
  activeSales().forEach((sale) => {
    const key = getSaleDate(sale).slice(0, 4);
    if (!key) return;
    totals.set(key, (totals.get(key) || 0) + Number(sale.total || 0));
  });

  return [...totals.keys()].sort().slice(-limit).map((key) => ({ key, label: key, value: roundMoney(totals.get(key)) }));
}

// Trend compares the most recent period against the one before it.
function computeTrend(series) {
  const hasRevenue = series.some((point) => point.value > 0);
  if (series.length < 2 || !hasRevenue) return { status: 'flat', percent: 0, hasData: false };

  const previous = series[series.length - 2].value;
  const current = series[series.length - 1].value;
  const diff = current - previous;
  if (Math.abs(diff) < 0.01) return { status: 'flat', percent: 0, hasData: true };

  const percent = previous > 0 ? Math.round(Math.abs(diff / previous) * 100) : 100;
  return { status: diff > 0 ? 'up' : 'down', percent, hasData: true };
}

function buildSalesChartSvg(series) {
  const width = 640;
  const height = 220;
  const padding = { top: 20, right: 10, bottom: 30, left: 10 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;
  const baselineY = padding.top + chartHeight;

  if (!series.length) {
    return `<text x="${width / 2}" y="${height / 2}" class="chart-empty-label">${escapeHtml(t('noSalesRecorded'))}</text>`;
  }

  const maxValue = Math.max(...series.map((point) => point.value), 1) * 1.15;
  const slot = chartWidth / series.length;
  const barWidth = Math.min(48, slot * 0.55);

  const gridlines = [0.25, 0.5, 0.75]
    .map((fraction) => {
      const y = padding.top + chartHeight * fraction;
      return `<line class="chart-gridline" x1="${padding.left}" x2="${width - padding.right}" y1="${y}" y2="${y}" />`;
    })
    .join('');

  const bars = series
    .map((point, index) => {
      const barHeight = Math.max(2, (point.value / maxValue) * chartHeight);
      const x = padding.left + index * slot + (slot - barWidth) / 2;
      const y = baselineY - barHeight;
      const isCurrent = index === series.length - 1;

      return `
        <rect class="chart-bar${isCurrent ? ' chart-bar-current' : ''}" x="${x}" y="${y}" width="${barWidth}" height="${barHeight}" rx="4">
          <title>${escapeHtml(point.label)}: ${escapeHtml(formatCurrency(point.value))}</title>
        </rect>
        <text class="chart-axis-label" x="${x + barWidth / 2}" y="${baselineY + 16}" text-anchor="middle">${escapeHtml(point.label)}</text>
      `;
    })
    .join('');

  return `
    <line class="chart-gridline" x1="${padding.left}" x2="${width - padding.right}" y1="${baselineY}" y2="${baselineY}" />
    ${gridlines}
    ${bars}
  `;
}

function renderSalesAnalysis() {
  const series = analysisPeriod === 'yearly' ? getYearlyRevenueSeries(5) : getMonthlyRevenueSeries(6);
  const trend = computeTrend(series);

  const badgeClass = !trend.hasData ? 'neutral' : trend.status === 'up' ? 'good' : trend.status === 'down' ? 'bad' : 'neutral';
  const badgeIcon = !trend.hasData ? '–' : trend.status === 'up' ? '▲' : trend.status === 'down' ? '▼' : '–';
  const badgeText = !trend.hasData ? t('trendNoData') : trend.status === 'up' ? t('trendUp') : trend.status === 'down' ? t('trendDown') : t('trendFlat');

  trendBadge.className = `trend-badge ${badgeClass}`;
  trendBadge.textContent = `${badgeIcon} ${badgeText}`;

  trendDetail.textContent = !trend.hasData
    ? t('trendDetailNoData')
    : trend.status === 'up' ? t('trendDetailUp', { percent: trend.percent })
      : trend.status === 'down' ? t('trendDetailDown', { percent: trend.percent })
        : t('trendDetailFlat');

  salesChart.innerHTML = buildSalesChartSvg(series);
}

function renderDashboardStats() {
  totalProductsStat.textContent = String(state.products.length);
  lowStockStat.textContent = String(getLowStockCount());
  renderPaymentSummary();
  renderDashboardInsights();
  renderSalesAnalysis();
}

function renderInventoryTable() {
  renderCategoryOptions();

  if (!state.products.length) {
    inventoryTableBody.innerHTML = `<tr><td colspan="7">${escapeHtml(t('noProductsInventory'))}</td></tr>`;
    return;
  }

  const filter = stockFilterInput.value;
  const categoryFilter = categoryFilterInput.value;
  const sort = inventorySortInput.value;
  const visibleProducts = state.products
    .filter((product) => {
      if (categoryFilter !== 'all' && product.category !== categoryFilter) return false;
      if (filter === 'low') return product.stock > 0 && product.stock <= product.lowStockThreshold;
      if (filter === 'out') return product.stock === 0;
      if (filter === 'available') return product.stock > 0;
      return true;
    })
    .slice()
    .sort((a, b) => {
      if (sort === 'price-low') return a.price - b.price;
      if (sort === 'price-high') return b.price - a.price;
      if (sort === 'stock-low') return a.stock - b.stock;
      if (sort === 'stock-high') return b.stock - a.stock;
      return a.name.localeCompare(b.name);
    });

  if (!visibleProducts.length) {
    inventoryTableBody.innerHTML = `<tr><td colspan="7">${escapeHtml(t('noProductsMatchFilter'))}</td></tr>`;
    return;
  }

  inventoryTableBody.innerHTML = visibleProducts
    .map((product) => `
      <tr class="${product.stock === 0 ? 'out-of-stock-row' : product.stock <= product.lowStockThreshold ? 'low-stock-row' : ''}">
        <td>${escapeHtml(product.name)}</td>
        <td>${escapeHtml(product.category)}</td>
        <td>${escapeHtml(product.unit)}</td>
        <td>${formatCurrency(product.purchasePrice || 0)}</td>
        <td>${formatCurrency(product.price)}</td>
        <td><strong>${product.stock}</strong>${product.stock === 0 ? ` <span class="stock-badge out">${escapeHtml(t('outOfStockBadge'))}</span>` : product.stock <= product.lowStockThreshold ? ` <span class="stock-badge low">${escapeHtml(t('lowBadge'))}</span>` : ''}</td>
        <td>
          <div class="table-actions">
            <button type="button" class="small-btn" data-action="edit-product" data-id="${escapeHtml(product.id)}">${escapeHtml(t('update'))}</button>
            <button type="button" class="small-btn delete-btn" data-action="delete-product" data-id="${escapeHtml(product.id)}">${escapeHtml(t('delete'))}</button>
          </div>
        </td>
      </tr>
    `)
    .join('');
}

function renderSearchResults() {
  const query = searchInput.value.trim().toLowerCase();
  const matches = state.products
    .filter((product) => !query || product.name.toLowerCase().includes(query))
    .sort((a, b) => a.name.localeCompare(b.name));

  if (!matches.length) {
    searchResults.innerHTML = `<div class="bill-empty">${escapeHtml(query ? t('noMatchingProducts') : t('noProductsFound'))}</div>`;
    return;
  }

  searchResults.innerHTML = matches
    .map(
      (product) => `
        <div class="search-product-row">
          <div>
            <strong>${escapeHtml(product.name)}</strong>
          </div>
          <div>${formatCurrency(product.price)}</div>
          <div>${product.stock}</div>
          <button type="button" class="search-add-btn" data-action="add-search-product" data-id="${escapeHtml(product.id)}"${product.stock > 0 ? '' : ' disabled'}>${escapeHtml(t('addBtn'))}</button>
        </div>
      `
    )
    .join('');
}

// Category lists come from the defaults plus any category an existing product already uses.
// The three default categories are translated; any category a shop owner typed in
// themselves is free text, so it is shown exactly as they entered it.
const DEFAULT_CATEGORY_KEYS = { Hardware: 'categoryHardware', Paints: 'categoryPaints', Cenentry: 'categoryCenentry' };

function renderCategoryOptions() {
  const categories = [...new Set([...DEFAULT_CATEGORIES, ...state.products.map((product) => product.category)])]
    .filter(Boolean)
    .sort((a, b) => a.localeCompare(b));
  const selectedFilter = categoryFilterInput.value || 'all';
  const selectedCategory = productCategoryInput.value;
  const options = categories
    .map((category) => `<option value="${escapeHtml(category)}">${escapeHtml(DEFAULT_CATEGORY_KEYS[category] ? t(DEFAULT_CATEGORY_KEYS[category]) : category)}</option>`)
    .join('');

  categoryFilterInput.innerHTML = `<option value="all">${escapeHtml(t('allSections'))}</option>${options}`;
  categoryFilterInput.value = categories.includes(selectedFilter) ? selectedFilter : 'all';
  productCategoryInput.innerHTML = options;
  if (categories.includes(selectedCategory)) productCategoryInput.value = selectedCategory;
}

function renderBill() {
  const billQuantity = state.bill.reduce((sum, item) => sum + item.qty, 0);
  billTabCount.textContent = String(billQuantity);
  billTabCount.classList.toggle('hidden', billQuantity === 0);

  if (!state.bill.length) {
    billItemsList.innerHTML = `<div class="bill-empty">${escapeHtml(t('noProductsInBill'))}</div>`;
    renderDashboardStats();
    return;
  }

  billItemsList.innerHTML = state.bill
    .map((item) => {
      const product = getProductById(item.productId);
      if (!product) return '';
      const itemTotal = product.price * item.qty;

      return `
        <div class="bill-item-row">
          <div class="bill-meta">
            <strong>${escapeHtml(product.name)}</strong>
            <span>${escapeHtml(t('singleRate', { rate: formatCurrency(product.price) }))}</span>
          </div>
          <div class="qty-controls">
            <button type="button" class="qty-btn" data-action="decrease-bill-item" data-id="${escapeHtml(product.id)}">−</button>
            <input class="qty-input" type="number" min="1" max="${product.stock}" value="${item.qty}" data-action="set-bill-quantity" data-id="${escapeHtml(product.id)}" aria-label="${escapeHtml(t('quantityForAria', { name: product.name }))}" />
            <button type="button" class="qty-btn" data-action="increase-bill-item" data-id="${escapeHtml(product.id)}">+</button>
          </div>
          <div class="bill-meta">
            <strong>${formatCurrency(itemTotal)}</strong>
            <span>${escapeHtml(t('total'))}</span>
          </div>
          <button type="button" class="remove-btn" data-action="remove-bill-item" data-id="${escapeHtml(product.id)}">${escapeHtml(t('remove'))}</button>
        </div>
      `;
    })
    .join('');

  renderDashboardStats();
}

function addToBill(productId, quantity = 1) {
  const product = getProductById(productId);
  if (!product) return false;

  const existingItem = getBillItem(productId);
  const currentQty = existingItem ? existingItem.qty : 0;

  if (currentQty + quantity > product.stock) {
    window.alert(t('insufficientStock', { name: product.name, stock: product.stock }));
    return false;
  }

  if (existingItem) {
    existingItem.qty += quantity;
  } else {
    state.bill.push({ productId, qty: quantity });
  }

  renderBill();
  renderSearchResults();
  renderInventoryTable();
  searchInput.focus();
  searchInput.select();
  return true;
}

function addScannedProduct() {
  const scannedValue = quickScanInput.value.trim().toLowerCase();
  if (!scannedValue) return;

  const product = state.products.find((item) =>
    item.id.toLowerCase() === scannedValue || item.name.toLowerCase() === scannedValue
  );

  if (!product) {
    window.alert(t('productNotFound'));
    quickScanInput.select();
    return;
  }

  if (addToBill(product.id)) {
    quickScanInput.value = '';
  }
}

function setBillQuantity(productId, value) {
  const product = getProductById(productId);
  const item = getBillItem(productId);
  const quantity = Number(value);
  if (!product || !item) return;

  if (!Number.isInteger(quantity) || quantity < 1 || quantity > product.stock) {
    window.alert(t('quantityRange', { stock: product.stock }));
    renderBill();
    return;
  }

  item.qty = quantity;
  renderBill();
  renderSearchResults();
  renderInventoryTable();
}

function increaseBillItem(productId) {
  const product = getProductById(productId);
  if (!product) return;

  const item = getBillItem(productId);
  if (!item) return;

  if (item.qty + 1 > product.stock) {
    window.alert(t('insufficientStock', { name: product.name, stock: product.stock }));
    return;
  }

  item.qty += 1;
  renderBill();
  renderSearchResults();
  renderInventoryTable();
}

function decreaseBillItem(productId) {
  const item = getBillItem(productId);
  if (!item) return;

  item.qty -= 1;

  if (item.qty <= 0) {
    state.bill = state.bill.filter((billItem) => billItem.productId !== productId);
  }

  renderBill();
  renderSearchResults();
  renderInventoryTable();
}

function removeBillItem(productId) {
  state.bill = state.bill.filter((item) => item.productId !== productId);
  renderBill();
  renderSearchResults();
  renderInventoryTable();
}

function parseMoneyValue(value) {
  if (value === null || value === undefined) return NaN;

  const trimmed = String(value).trim().replace(/,/g, '');
  if (!trimmed) return NaN;

  const normalized = trimmed.replace(/^0+(?=\d)/, '');
  const match = normalized.match(/^\d+(?:\.\d+)?$/);
  if (!match) return NaN;

  const parsed = Number(normalized);
  return Number.isFinite(parsed) ? parsed : NaN;
}

// While typing: only drop characters that can never be part of a price.
// (Tidying up on every keystroke deleted a trailing "." or "0", so 12.5 or 12.05 could not be typed.)
function sanitizeMoneyInput(input) {
  const [whole, ...fractions] = input.value.replace(/[^0-9.]/g, '').split('.');
  const cleaned = fractions.length ? `${whole}.${fractions.join('')}` : whole;
  if (cleaned !== input.value) input.value = cleaned;
}

// When the field loses focus: remove leading zeros and a trailing dot.
function normalizeMoneyInput(input) {
  const parsed = parseMoneyValue(input.value);
  input.value = Number.isFinite(parsed) ? String(parsed) : '';
}

function showTab(tabName) {
  const activeTab = TAB_NAMES.includes(tabName) ? tabName : 'dashboard';
  appLayout.dataset.activeTab = activeTab;
  appLayout.querySelectorAll('[data-tab]').forEach((section) => {
    section.classList.toggle('tab-hidden', section.dataset.tab !== activeTab);
  });
  tabButtons.forEach((button) => {
    const isActive = button.dataset.tabTarget === activeTab;
    button.classList.toggle('active', isActive);
    button.setAttribute('aria-selected', String(isActive));
  });

  try {
    localStorage.setItem(STORAGE_KEYS.activeTab, activeTab);
  } catch (error) {
    // Remembering the tab is only a convenience.
  }
}

function getSavedTab() {
  try {
    return localStorage.getItem(STORAGE_KEYS.activeTab) || 'dashboard';
  } catch (error) {
    return 'dashboard';
  }
}

function clearForm() {
  productForm.reset();
  state.editingProductId = null;
  saveProductBtn.textContent = t('saveProduct');
  productFormTitle.textContent = t('addProduct');
  cancelEditBtn.classList.add('hidden');
  setFormMessage('');
}

async function handleProductSubmit(event) {
  event.preventDefault();

  const name = productNameInput.value.trim();
  const price = parseMoneyValue(productPriceInput.value);
  const purchasePrice = parseMoneyValue(productPurchasePriceInput.value || '0');
  const stock = productStockInput.value.trim() === '' ? NaN : Number(productStockInput.value);
  const category = productCategoryInput.value.trim() || 'General';
  const unit = productUnitInput.value || 'piece';
  const lowStockThreshold = Number(productThresholdInput.value);

  if (!name) {
    setFormMessage(t('pleaseEnterName'), 'error');
    return;
  }

  if (!Number.isFinite(price) || price <= 0) {
    setFormMessage(t('pleaseEnterValidPrice'), 'error');
    return;
  }

  if (!Number.isFinite(purchasePrice) || purchasePrice < 0) {
    setFormMessage(t('pleaseEnterValidPurchasePrice'), 'error');
    return;
  }

  if (!Number.isInteger(stock) || stock < 0) {
    setFormMessage(t('pleaseEnterValidStock'), 'error');
    return;
  }

  if (!Number.isInteger(lowStockThreshold) || lowStockThreshold < 0) {
    setFormMessage(t('pleaseEnterValidThreshold'), 'error');
    return;
  }

  const existingProduct = state.products.find((product) => (
    product.id !== state.editingProductId && product.name.toLowerCase() === name.toLowerCase()
  ));
  if (existingProduct) {
    setFormMessage(state.editingProductId
      ? t('duplicateNameEditing', { name: existingProduct.name })
      : t('duplicateNameAdding', { name: existingProduct.name }), 'error');
    return;
  }

  try {
    const wasEditing = Boolean(state.editingProductId);
    if (wasEditing) {
      await apiFetch(`/products/${state.editingProductId}`, {
        method: 'PUT',
        body: JSON.stringify({ name, price, purchasePrice, stock, category, unit, lowStockThreshold }),
      });
    } else {
      await apiFetch('/products', {
        method: 'POST',
        body: JSON.stringify({ name, price, purchasePrice, stock, category, unit, lowStockThreshold }),
      });
    }

    await refreshProducts();
    // clearForm() wipes the message, so show the success text after it.
    clearForm();
    setFormMessage(wasEditing ? t('productUpdated') : t('productSaved'), 'success');
  } catch (error) {
    setFormMessage(error.message || t('unableToSaveProduct'), 'error');
  }
}

function populateEditForm(productId) {
  const product = getProductById(productId);
  if (!product) return;

  state.editingProductId = productId;
  productNameInput.value = product.name;
  productPriceInput.value = product.price;
  productPurchasePriceInput.value = product.purchasePrice || '';
  productStockInput.value = product.stock;
  renderCategoryOptions();
  productCategoryInput.value = product.category || 'General';
  productUnitInput.value = product.unit || 'piece';
  productThresholdInput.value = product.lowStockThreshold ?? 5;
  saveProductBtn.textContent = t('updateProduct');
  cancelEditBtn.classList.remove('hidden');
  setFormMessage(t('editingProductDetails'), 'success');
  productFormTitle.textContent = t('updateProduct');
  showTab('add-product');
  productNameInput.focus();
}

async function deleteProduct(productId) {
  const product = getProductById(productId);
  if (!product) return;

  const confirmed = window.confirm(t('confirmDeleteProduct', { name: product.name }));
  if (!confirmed) return;

  try {
    await apiFetch(`/products/${productId}`, { method: 'DELETE' });
    await refreshProducts();

    if (state.editingProductId === productId) {
      clearForm();
    }

    state.bill = state.bill.filter((item) => item.productId !== productId);
    renderBill();
  } catch (error) {
    setFormMessage(error.message || t('unableToDeleteProduct'), 'error');
  }
}

function buildReceiptHtml(receiptData) {
  const rows = (receiptData.items || [])
    .map(
      (item) => `
        <tr>
          <td>${escapeHtml(item.name)}</td>
          <td>${formatAmount(item.rate)}</td>
          <td>${escapeHtml(item.qty)}</td>
          <td>${formatAmount(item.total)}</td>
        </tr>
      `
    )
    .join('');

  return `
    <div class="receipt">
      ${receiptData.voided ? `<div class="receipt-voided-banner">${escapeHtml(t('voidedBannerText'))}</div>` : ''}
      <h3>${escapeHtml((receiptData.shopName || state.shopName).toUpperCase())}</h3>
      ${receiptData.shopAddress ? `<div class="receipt-shop-detail">${escapeHtml(receiptData.shopAddress)}</div>` : ''}
      ${receiptData.shopPhone ? `<div class="receipt-shop-detail">${escapeHtml(t('phonePrefixed', { phone: receiptData.shopPhone }))}</div>` : ''}
      <div class="receipt-meta">
        <span>${escapeHtml(t('billNoPrefixed', { num: receiptData.billNo || t('na') }))}</span>
        <span>${escapeHtml(t('datePrefixed', { date: receiptData.createdAt || new Date().toLocaleString() }))}</span>
      </div>
      <div class="receipt-customer">
        <span>${escapeHtml(t('customerPrefixed', { name: receiptData.customerName || t('walkInCustomer') }))}</span>
        <span>${escapeHtml(t('phonePrefixed', { phone: receiptData.customerPhone || t('na') }))}</span>
        <span>${escapeHtml(t('paymentPrefixed', { method: receiptData.paymentMethod || t('paymentCash') }))}</span>
      </div>
      <hr />
      <table class="receipt-table">
        <thead>
          <tr>
            <th>${escapeHtml(t('receiptColProduct'))}</th>
            <th>${escapeHtml(t('receiptColRate'))}</th>
            <th>${escapeHtml(t('receiptColQty'))}</th>
            <th>${escapeHtml(t('receiptColTotal'))}</th>
          </tr>
        </thead>
        <tbody>
          ${rows}
        </tbody>
      </table>
      <hr />
      <div class="receipt-lines">
        <div><span>${escapeHtml(t('subtotal'))}</span><strong>${formatCurrency(receiptData.subtotal || 0)}</strong></div>
        <div><span>${escapeHtml(t('discount'))}</span><strong>${formatCurrency(receiptData.discount || 0)}</strong></div>
        <div><span>${escapeHtml(t('taxWithRate', { rate: Number(receiptData.taxRate || 0).toFixed(2) }))}</span><strong>${formatCurrency(receiptData.tax || 0)}</strong></div>
        <div class="receipt-total"><span>${escapeHtml(t('totalAmount'))}</span><strong>${formatCurrency(receiptData.total || 0)}</strong></div>
        <div><span>${escapeHtml(t('amountPaid'))}</span><strong>${formatCurrency(receiptData.amountPaid || 0)}</strong></div>
        <div><span>${escapeHtml((receiptData.change || 0) < 0 ? t('stillDueLabel') : t('change'))}</span><strong>${formatCurrency(Math.abs(receiptData.change || 0))}</strong></div>
        ${receiptData.customerId ? `<div><span>${escapeHtml(t('creditCustomerLabel'))}</span><strong>${escapeHtml(receiptData.customerName || '')}</strong></div>` : ''}
      </div>
      <div class="receipt-actions">
        <button type="button" class="secondary-btn" data-action="print-receipt">${escapeHtml(t('printReceiptBtn'))}</button>
        <button type="button" class="secondary-btn" data-action="download-receipt">${escapeHtml(t('downloadAsText'))}</button>
        ${receiptData.id && !receiptData.voided ? `
        <button type="button" class="secondary-btn" data-action="add-items-to-sale" data-id="${escapeHtml(receiptData.id)}">${escapeHtml(t('addMoreItemsBtn'))}</button>
        <button type="button" class="small-btn delete-btn" data-action="void-sale" data-id="${escapeHtml(receiptData.id)}">${escapeHtml(t('voidBillBtn'))}</button>
        ` : ''}
      </div>
    </div>
  `;
}

// state.lastReceipt always matches the receipt on screen, so export uses the bill being viewed.
function showReceipt(receipt) {
  state.lastReceipt = receipt;
  receiptContainer.innerHTML = buildReceiptHtml(receipt);
}

function exportReceiptFile() {
  const receipt = state.lastReceipt;

  if (!receipt || !receipt.items || !receipt.items.length) {
    window.alert(t('noReceiptToExport'));
    return;
  }

  const lines = [
    (receipt.shopName || state.shopName).toUpperCase(),
    ...(receipt.shopAddress ? [t('addressPrefixed', { address: receipt.shopAddress })] : []),
    ...(receipt.shopPhone ? [t('phonePrefixed', { phone: receipt.shopPhone })] : []),
    t('billNoPrefixed', { num: receipt.billNo || t('na') }),
    t('datePrefixed', { date: receipt.createdAt || '' }),
    t('customerPrefixed', { name: receipt.customerName || t('walkInCustomer') }),
    t('paymentPrefixed', { method: receipt.paymentMethod || t('paymentCash') }),
    '================================',
    ...receipt.items.map((item) => `${item.name} | ${t('receiptColRate')}: ${formatCurrency(item.rate)} | ${t('receiptColQty')}: ${item.qty} | ${t('receiptColTotal')}: ${formatCurrency(item.total)}`),
    '================================',
    `${t('subtotal')}: ${formatCurrency(receipt.subtotal || 0)}`,
    `${t('discount')}: ${formatCurrency(receipt.discount || 0)}`,
    `${t('taxWithRate', { rate: Number(receipt.taxRate || 0).toFixed(2) })}: ${formatCurrency(receipt.tax || 0)}`,
    `${t('totalAmount')}: ${formatCurrency(receipt.total || 0)}`,
    `${t('amountPaid')}: ${formatCurrency(receipt.amountPaid || 0)}`,
    `${(receipt.change || 0) < 0 ? t('stillDueLabel') : t('change')}: ${formatCurrency(Math.abs(receipt.change || 0))}`,
  ];

  const blob = new Blob([`${lines.join('\n')}\n`], { type: 'text/plain;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  link.download = `${(receipt.shopName || state.shopName).replace(/\s+/g, '_').toLowerCase()}_receipt_${stamp}.txt`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function exportSalesCsv() {
  const sales = activeSales();
  if (!sales.length) {
    window.alert(t('noSalesToExport'));
    return;
  }

  // Text starting with = + - @ (or tab/CR) would run as a formula in Excel, so it gets a leading apostrophe.
  const escapeCsv = (value) => {
    let text = String(value ?? '');
    if (typeof value === 'string' && /^[=+\-@\t\r]/.test(text)) text = `'${text}`;
    return `"${text.replace(/"/g, '""')}"`;
  };
  const rows = [[
    t('csvHeaderBillNo'), t('csvHeaderDate'), t('csvHeaderShop'), t('csvHeaderProduct'), t('csvHeaderQuantity'),
    t('csvHeaderSellingRate'), t('csvHeaderPurchaseRate'), t('csvHeaderSalesTotal'), t('csvHeaderProfit'),
    t('csvHeaderPaymentMethod'), t('csvHeaderCustomer'),
  ]];

  sales.forEach((sale) => {
    (sale.items || []).forEach((item) => {
      const quantity = Number(item.qty || 0);
      const rate = Number(item.rate || 0);
      const purchaseRate = Number(item.purchasePrice || 0);
      const itemProfit = Number(item.profit ?? ((rate - purchaseRate) * quantity));
      rows.push([
        sale.billNo || sale.id || '',
        sale.createdAt || '',
        sale.shopName || state.shopName,
        item.name || '',
        quantity,
        rate,
        purchaseRate,
        Number(item.total || rate * quantity),
        itemProfit,
        sale.paymentMethod || t('paymentCash'),
        sale.customerName || t('walkInCustomer'),
      ]);
    });
  });

  const csv = rows.map((row) => row.map(escapeCsv).join(',')).join('\r\n');
  const blob = new Blob([`\ufeff${csv}\r\n`], { type: 'text/csv;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = `sales_report_${new Date().toISOString().slice(0, 10)}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function printReceipt() {
  const receipt = receiptContainer.querySelector('.receipt');
  if (!receipt) return;

  const printWindow = window.open('', '_blank', 'width=800,height=900');
  if (!printWindow) {
    window.alert(t('allowPopupsToPrint'));
    return;
  }

  const paperWidth = state.printerSize === '58' ? '58mm' : '80mm';
  const contentWidth = state.printerSize === '58' ? '50mm' : '72mm';
  printWindow.document.write(`<!DOCTYPE html><html><head><title>${escapeHtml(t('receiptWindowTitle'))}</title><style>@page{size:${paperWidth} auto;margin:4mm}body{font-family:Consolas,"Courier New",monospace;width:${contentWidth};margin:0;color:#111;font-size:11px}.receipt{width:100%;margin:0}.receipt h3{text-align:center;font-size:15px;margin:0 0 8px}.receipt-meta,.receipt-lines div{display:flex;justify-content:space-between;gap:8px;margin:6px 0}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:4px 2px;border-bottom:1px dashed #aaa}th:last-child,td:last-child{text-align:right}.receipt-actions{display:none}</style></head><body>${receipt.outerHTML}</body></html>`);
  printWindow.document.close();
  printWindow.focus();
  printWindow.print();
}

function startNewBill() {
  state.bill = [];
  state.editingSaleId = null;
  discountInput.value = '0';
  taxInput.value = '0';
  amountPaidInput.value = '';
  paymentMethodInput.value = 'Cash';
  customerNameInput.value = '';
  customerPhoneInput.value = '';
  creditCustomerInput.value = '';
  updateCreditUi();
  [discountInput, taxInput, paymentMethodInput, customerNameInput, customerPhoneInput, creditCustomerInput].forEach((input) => {
    input.disabled = false;
  });
  generateBillBtn.textContent = t('generateBill');
  billEditBanner.classList.add('hidden');
  receiptContainer.innerHTML = '';
  state.lastReceipt = null;
  generateBillBtn.classList.remove('hidden');
  newBillBtn.classList.add('hidden');
  renderBill();
  renderSearchResults();
}

function startNewDay() {
  const confirmed = window.confirm(t('confirmStartNewDay'));
  if (!confirmed) return;

  startNewBill();
  state.businessDayStartedAt = Date.now();
  localStorage.setItem(STORAGE_KEYS.businessDayStartedAt, String(state.businessDayStartedAt));
  clearForm();
  searchInput.value = '';
  quickScanInput.value = '';
  dayResetMessage.textContent = t('newDayStarted');
  dayResetMessage.className = 'form-message success';
  renderDashboardStats();
}

async function backupNow() {
  backupNowBtn.disabled = true;
  const originalText = backupNowBtn.textContent;
  backupNowBtn.textContent = t('backingUp');

  try {
    await apiFetch('/backup', { method: 'POST' });
    backupNowBtn.textContent = t('backupComplete');
    window.setTimeout(() => {
      backupNowBtn.textContent = originalText;
      backupNowBtn.disabled = false;
    }, 1800);
  } catch (error) {
    backupNowBtn.textContent = originalText;
    backupNowBtn.disabled = false;
    window.alert(error.message || t('unableToCreateBackup'));
  }
}

function resetMonthlyProfit() {
  const confirmed = window.confirm(t('confirmResetMonthlyProfit'));
  if (!confirmed) return;

  state.monthlyProfitStartedAt = Date.now();
  localStorage.setItem(STORAGE_KEYS.monthlyProfitStartedAt, String(state.monthlyProfitStartedAt));
  renderDashboardStats();
}

function renderSalesHistory() {
  if (!state.sales.length) {
    salesHistoryList.innerHTML = `<div class="bill-empty">${escapeHtml(t('noPreviousBills'))}</div>`;
    return;
  }

  salesHistoryList.innerHTML = state.sales
    .map((sale) => `
      <article class="saved-sale${sale.voided ? ' voided-sale' : ''}">
        <div class="saved-sale-heading">
          <strong><span class="bill-no-tag">${escapeHtml(t('billNumberPrefix', { num: sale.billNo }))}</span> ${escapeHtml(sale.createdAt || t('savedBill'))}${sale.voided ? ` <span class="voided-tag">${escapeHtml(t('voidedTag'))}</span>` : ''}</strong>
          <strong>${formatCurrency(sale.total || 0)}</strong>
        </div>
        <span>${escapeHtml(sale.shopName || state.shopName)} · ${escapeHtml(t('itemsSuffix', { count: (sale.items || []).reduce((sum, item) => sum + Number(item.qty || 0), 0) }))} · ${escapeHtml(sale.paymentMethod || t('paymentCash'))} · ${escapeHtml(sale.customerName || t('walkInCustomer'))}</span>
        <button type="button" class="small-btn" data-action="view-sale" data-id="${escapeHtml(sale.id)}">${escapeHtml(t('viewBill'))}</button>
        ${sale.voided ? '' : `
        <button type="button" class="small-btn" data-action="add-items-to-sale" data-id="${escapeHtml(sale.id)}">${escapeHtml(t('addMoreItemsBtn'))}</button>
        <button type="button" class="small-btn delete-btn" data-action="void-sale" data-id="${escapeHtml(sale.id)}">${escapeHtml(t('voidBillBtn'))}</button>
        `}
      </article>
    `)
    .join('');
}

// ---------------------------------------------------------------- credit customers ("khata")

async function loadCustomers() {
  try {
    const list = await apiFetch('/customers');
    state.customers = Array.isArray(list) ? list : [];
  } catch (error) {
    state.customers = [];
  }
}

function renderCustomersList() {
  if (!state.customers.length) {
    customersList.innerHTML = `<div class="customer-empty">${escapeHtml(t('noCustomersYet'))}</div>`;
    return;
  }

  customersList.innerHTML = state.customers
    .map((customer) => {
      const balance = Number(customer.balance || 0);
      const owes = balance > 0.001;
      const inCredit = balance < -0.001;
      const balanceLabel = owes
        ? t('customerOwes', { amount: formatCurrency(balance) })
        : inCredit
          ? t('customerInCredit', { amount: formatCurrency(Math.abs(balance)) })
          : t('customerSettled');
      return `
        <article class="customer-card">
          <div class="customer-card-heading">
            <strong>${escapeHtml(customer.name)}</strong>
            <span class="customer-balance ${owes ? 'owes' : 'settled'}">${escapeHtml(balanceLabel)}</span>
          </div>
          <span>${escapeHtml(customer.phone ? t('phonePrefixed', { phone: customer.phone }) : t('noPhoneOnFile'))}</span>
          <div class="customer-card-actions">
            <button type="button" class="small-btn" data-action="record-customer-payment" data-id="${escapeHtml(customer.id)}">${escapeHtml(t('recordPaymentBtn'))}</button>
            <button type="button" class="small-btn" data-action="edit-customer" data-id="${escapeHtml(customer.id)}">${escapeHtml(t('editCustomerBtn'))}</button>
          </div>
        </article>
      `;
    })
    .join('');
}

// Keeps the Bill tab's credit-customer dropdown in sync with state.customers - called
// after every customer add/edit/payment, not just on boot.
function populateCreditCustomerSelect() {
  const previousValue = creditCustomerInput.value;
  creditCustomerInput.innerHTML = `<option value="">${escapeHtml(t('selectCustomerOption'))}</option>` +
    state.customers.map((customer) => `<option value="${escapeHtml(customer.id)}">${escapeHtml(customer.name)}</option>`).join('');
  if (state.customers.some((customer) => customer.id === previousValue)) {
    creditCustomerInput.value = previousValue;
  }
}

function setCustomerFormMessage(message, type = '') {
  customerFormMessage.textContent = message;
  customerFormMessage.className = 'form-message';
  if (type) customerFormMessage.classList.add(type);
}

async function handleAddCustomer(event) {
  event.preventDefault();

  const name = newCustomerNameInput.value.trim();
  const phone = newCustomerPhoneInput.value.trim();
  if (!name) {
    setCustomerFormMessage(t('pleaseEnterCustomerName'), 'error');
    return;
  }

  try {
    await apiFetch('/customers', {
      method: 'POST',
      body: JSON.stringify({ name, phone }),
    });
    addCustomerForm.reset();
    setCustomerFormMessage(t('customerAdded'), 'success');
    await loadCustomers();
    renderCustomersList();
    populateCreditCustomerSelect();
    renderDashboardStats();
  } catch (error) {
    setCustomerFormMessage(error.message || t('unableToAddCustomer'), 'error');
  }
}

async function recordCustomerPayment(customerId) {
  const customer = state.customers.find((item) => item.id === customerId);
  if (!customer) return;

  const amountText = window.prompt(
    t('promptPaymentAmount', { name: customer.name, balance: formatCurrency(customer.balance || 0) }), ''
  );
  if (amountText === null) return;
  const amount = parseMoneyValue(amountText);
  if (!Number.isFinite(amount) || amount <= 0) {
    window.alert(t('invalidPaymentAmount'));
    return;
  }
  const note = window.prompt(t('promptPaymentNote'), '') || '';

  try {
    await apiFetch(`/customers/${encodeURIComponent(customerId)}/payment`, {
      method: 'POST',
      body: JSON.stringify({ amount, note }),
    });
    await loadCustomers();
    renderCustomersList();
    renderDashboardStats();
    window.alert(t('paymentRecorded'));
  } catch (error) {
    window.alert(error.message || t('unableToRecordPayment'));
  }
}

async function editCustomer(customerId) {
  const customer = state.customers.find((item) => item.id === customerId);
  if (!customer) return;

  const name = window.prompt(t('promptCustomerName'), customer.name);
  if (name === null) return;
  if (!name.trim()) {
    window.alert(t('pleaseEnterCustomerName'));
    return;
  }
  const phone = window.prompt(t('promptCustomerPhone'), customer.phone || '');
  if (phone === null) return;

  try {
    await apiFetch(`/customers/${encodeURIComponent(customerId)}`, {
      method: 'PUT',
      body: JSON.stringify({ name: name.trim(), phone: phone.trim() }),
    });
    await loadCustomers();
    renderCustomersList();
    populateCreditCustomerSelect();
  } catch (error) {
    window.alert(error.message || t('unableToUpdateCustomer'));
  }
}

// Shows/hides the credit-customer picker vs. the free-text customer name/phone fields
// based on the selected payment method. Called on paymentMethodInput change and whenever
// the Bill tab's payment fields are reset (startNewBill/cancelBillEdit).
function updateCreditUi() {
  const isCredit = paymentMethodInput.value === 'Credit';
  creditCustomerGroup.classList.toggle('hidden', !isCredit);
  customerNameGroup.classList.toggle('hidden', isCredit);
  customerPhoneGroup.classList.toggle('hidden', isCredit);
  amountPaidLabel.textContent = isCredit ? t('creditAmountPaidLabel') : t('amountPaidLabel');
  amountPaidInput.placeholder = isCredit ? t('creditAmountPaidPlaceholder') : t('amountPaidPlaceholder');
}

// Switches the Bill tab into "edit" mode: the current bill list is repurposed to hold the
// extra items being added, discount/tax/payment/customer are locked to the original sale's
// values (the backend keeps them as-is), and Amount Paid is re-entered as the new grand
// total paid so far for the whole bill.
function startAddItemsToSale(saleId) {
  const sale = state.sales.find((savedSale) => savedSale.id === saleId);
  if (!sale) return;

  if (state.bill.length && state.editingSaleId !== saleId) {
    window.alert(t('finishCurrentBillFirst'));
    return;
  }

  state.editingSaleId = saleId;
  state.bill = [];

  discountInput.value = String(sale.discount || 0);
  taxInput.value = String(sale.taxRate || 0);
  paymentMethodInput.value = sale.paymentMethod || 'Cash';
  customerNameInput.value = sale.customerName || '';
  customerPhoneInput.value = sale.customerPhone || '';
  creditCustomerInput.value = sale.customerId || '';
  updateCreditUi();
  amountPaidInput.value = '';

  // Discount/tax stay editable - the cashier can adjust either for the whole bill.
  // Payment method/customer stay locked: the backend keeps those as-is on an update.
  [paymentMethodInput, customerNameInput, customerPhoneInput, creditCustomerInput].forEach((input) => {
    input.disabled = true;
  });

  generateBillBtn.textContent = t('saveAddedItems');
  generateBillBtn.classList.remove('hidden');
  newBillBtn.classList.add('hidden');
  billEditBanner.classList.remove('hidden');
  billEditBannerText.textContent = t('addingItemsToBillNotice', { num: sale.billNo, paid: formatCurrency(sale.amountPaid || 0) });

  showReceipt(sale);
  showTab('bill');
  renderBill();
  renderPaymentSummary();
  searchInput.focus();
}

function cancelBillEdit() {
  state.editingSaleId = null;
  state.bill = [];

  [paymentMethodInput, customerNameInput, customerPhoneInput, creditCustomerInput].forEach((input) => {
    input.disabled = false;
  });
  discountInput.value = '0';
  taxInput.value = '0';
  paymentMethodInput.value = 'Cash';
  customerNameInput.value = '';
  customerPhoneInput.value = '';
  creditCustomerInput.value = '';
  updateCreditUi();
  amountPaidInput.value = '';

  generateBillBtn.textContent = t('generateBill');
  billEditBanner.classList.add('hidden');

  renderBill();
  renderPaymentSummary();
}

async function saveAddedItems() {
  const saleId = state.editingSaleId;
  const sale = state.sales.find((savedSale) => savedSale.id === saleId);
  if (!sale) {
    cancelBillEdit();
    return;
  }

  const details = getPaymentDetails();
  // A bill already linked to a credit customer can carry a balance, same exception as a
  // brand new credit sale - see generateBill().
  if (!sale.customerId && details.amountPaid + 0.001 < details.total) {
    window.alert(t('amountPaidLessThanTotal', { amount: formatCurrency(details.total) }));
    return;
  }

  billInProgress = true;
  generateBillBtn.disabled = true;
  try {
    const updatedSale = await apiFetch(`/sale/${encodeURIComponent(saleId)}`, {
      method: 'POST',
      body: JSON.stringify({
        items: state.bill.map((item) => ({ productId: item.productId, qty: item.qty })),
        amountPaid: details.amountPaid,
        discount: details.discount,
        taxRate: details.taxRate,
      }),
    });

    const index = state.sales.findIndex((savedSale) => savedSale.id === saleId);
    if (index !== -1) state.sales[index] = updatedSale;
    persistSalesLocal();
    renderSalesHistory();
    showReceipt(updatedSale);

    cancelBillEdit();
    await refreshProducts();
    renderBill();
    renderSearchResults();
    if (updatedSale.customerId) {
      await loadCustomers();
      renderCustomersList();
      renderDashboardStats();
    }
  } catch (error) {
    window.alert(error.message || t('unableToAddItemsToBill'));
  } finally {
    billInProgress = false;
    generateBillBtn.disabled = false;
  }
}

// Cancels a saved bill: the backend restores stock for every line and flags the record
// as voided (it stays in history for the books, just excluded from revenue/profit).
async function voidSale(saleId) {
  const sale = state.sales.find((savedSale) => savedSale.id === saleId);
  if (!sale || sale.voided) return;

  const confirmed = window.confirm(t('confirmVoidBill', { num: sale.billNo, amount: formatCurrency(sale.total || 0) }));
  if (!confirmed) return;

  const reason = window.prompt(t('voidReasonPrompt'), '') || '';

  try {
    const updatedSale = await apiFetch(`/sale/${encodeURIComponent(saleId)}/void`, {
      method: 'POST',
      body: JSON.stringify({ reason }),
    });

    const index = state.sales.findIndex((savedSale) => savedSale.id === saleId);
    if (index !== -1) state.sales[index] = updatedSale;
    persistSalesLocal();

    if (state.editingSaleId === saleId) cancelBillEdit();
    if (state.lastReceipt && state.lastReceipt.id === saleId) showReceipt(updatedSale);

    renderSalesHistory();
    await refreshProducts();

    if (updatedSale.customerId) {
      await loadCustomers();
      renderCustomersList();
      renderDashboardStats();
    }

    if (updatedSale.restockWarnings && updatedSale.restockWarnings.length) {
      window.alert(t('voidRestockWarning', { items: updatedSale.restockWarnings.join(', ') }));
    } else {
      window.alert(t('billVoided'));
    }
  } catch (error) {
    window.alert(error.message || t('unableToVoidBill'));
  }
}

async function generateBill() {
  // Ignore a second click / Ctrl+Enter while the first bill is still being saved.
  if (billInProgress) return;

  // Editing a saved bill can be just a discount/tax/amount-paid correction, so it's
  // allowed to go through with no new items - unlike starting a brand new bill below.
  if (state.editingSaleId) {
    await saveAddedItems();
    return;
  }

  if (!state.bill.length) {
    window.alert(t('noProductsInCurrentBill'));
    return;
  }

  const receiptSnapshot = {
    shopName: state.shopName,
    shopPhone: state.shopPhone,
    shopAddress: state.shopAddress,
    items: state.bill.map((item) => {
      const product = getProductById(item.productId);
      return {
        name: product ? product.name : 'Unknown',
        rate: product ? product.price : 0,
        qty: item.qty,
        total: product ? product.price * item.qty : 0,
      };
    }),
    ...getPaymentDetails(),
  };

  const isCreditSale = receiptSnapshot.paymentMethod === 'Credit';
  if (isCreditSale && !receiptSnapshot.customerId) {
    window.alert(t('creditSaleNeedsCustomer'));
    return;
  }
  // A credit sale can be left partly (or entirely) unpaid - the shortfall becomes the
  // customer's balance - so only a non-credit sale still has to be paid in full here.
  if (!isCreditSale && receiptSnapshot.amountPaid < receiptSnapshot.total) {
    window.alert(t('amountPaidLessThanTotal', { amount: formatCurrency(receiptSnapshot.total) }));
    return;
  }

  billInProgress = true;
  generateBillBtn.disabled = true;
  try {
    const saleResult = await apiFetch('/sale', {
      method: 'POST',
      body: JSON.stringify({
        items: state.bill.map((item) => ({
          productId: item.productId,
          qty: item.qty,
        })),
        discount: receiptSnapshot.discount,
        taxRate: receiptSnapshot.taxRate,
        amountPaid: receiptSnapshot.amountPaid,
        paymentMethod: receiptSnapshot.paymentMethod,
        customerName: receiptSnapshot.customerName,
        customerPhone: receiptSnapshot.customerPhone,
        customerId: receiptSnapshot.customerId,
      }),
    });

    // The backend's numbers are what was saved, so they take priority over the local snapshot.
    showReceipt({ ...receiptSnapshot, ...saleResult });

    state.sales.unshift(saleResult);
    persistSalesLocal();
    renderSalesHistory();

    state.bill = [];
    generateBillBtn.classList.add('hidden');
    newBillBtn.classList.remove('hidden');
    await refreshProducts();
    renderBill();
    renderSearchResults();
    if (isCreditSale) {
      await loadCustomers();
      renderCustomersList();
      renderDashboardStats();
    }
  } catch (error) {
    window.alert(error.message || t('unableToGenerateBill'));
  } finally {
    billInProgress = false;
    generateBillBtn.disabled = false;
  }
}

async function refreshProducts() {
  try {
    const products = await apiFetch('/products');
    state.products = Array.isArray(products) ? products.map(normalizeProduct) : [];
    persistProductsLocal();
  } catch (error) {
    persistProductsLocal();
  }

  renderInventoryTable();
  renderSearchResults();
  renderDashboardStats();
}

// Re-renders everything that embeds translated text or a formatted currency amount,
// so switching language or currency updates the whole page immediately instead of
// only the parts that happen to re-render on their own next.
function refreshDynamicViews() {
  setConnectionStatus(lastConnectionStatus);
  // productFormTitle/saveProductBtn carry a static data-i18n for their default text, but
  // flip to "Update Product" while editing - re-derive that from state so a language/
  // currency switch mid-edit doesn't silently revert the form to "Add Product".
  productFormTitle.textContent = state.editingProductId ? t('updateProduct') : t('addProduct');
  saveProductBtn.textContent = state.editingProductId ? t('updateProduct') : t('saveProduct');
  // generateBillBtn/billEditBannerText carry their own default data-i18n text, but flip
  // while adding items to a saved bill - re-derive that the same way as the product form above.
  if (state.editingSaleId) {
    const editingSale = state.sales.find((sale) => sale.id === state.editingSaleId);
    generateBillBtn.textContent = t('saveAddedItems');
    if (editingSale) {
      billEditBannerText.textContent = t('addingItemsToBillNotice', { num: editingSale.billNo, paid: formatCurrency(editingSale.amountPaid || 0) });
    }
  }
  updateCreditUi();
  updateShopDisplay();
  renderCategoryOptions();
  renderInventoryTable();
  renderSearchResults();
  renderBill();
  renderDashboardStats();
  renderSalesHistory();
  renderCustomersList();
  populateCreditCustomerSelect();
  if (state.lastReceipt) showReceipt(state.lastReceipt);
}

function setChangePasswordMessage(message, type = '') {
  changePasswordMessage.textContent = message;
  changePasswordMessage.className = 'form-message';
  if (type) changePasswordMessage.classList.add(type);
}

async function handleChangePassword(event) {
  event.preventDefault();

  const currentPassword = currentPasswordInput.value;
  const newPassword = newPasswordInput.value;
  const confirmNewPassword = confirmNewPasswordInput.value;

  if (!currentPassword || !newPassword || !confirmNewPassword) {
    setChangePasswordMessage(t('pleaseFillAllPasswordFields'), 'error');
    return;
  }
  if (newPassword !== confirmNewPassword) {
    setChangePasswordMessage(t('newPasswordsDoNotMatch'), 'error');
    return;
  }

  changePasswordBtn.disabled = true;
  try {
    // A successful change revokes every existing session (including this one), so the
    // response carries a fresh token - without it the very next request would bounce
    // us to the login screen right after the change succeeded.
    const result = await apiFetch('/auth/change-password', {
      method: 'POST',
      body: JSON.stringify({ currentPassword, newPassword }),
    });
    setAuthToken(result.token);
    changePasswordForm.reset();
    setChangePasswordMessage(t('passwordChanged'), 'success');
  } catch (error) {
    setChangePasswordMessage(error.message || t('unableToChangePassword'), 'error');
  } finally {
    changePasswordBtn.disabled = false;
  }
}

function setRecoveryQuestionMessage(message, type = '') {
  recoveryQuestionMessage.textContent = message;
  recoveryQuestionMessage.className = 'form-message';
  if (type) recoveryQuestionMessage.classList.add(type);
}

// Shown as context above the Recovery Question form every time Shop Settings is opened,
// so the owner can see whether one is already set (and what it is) before overwriting it.
async function loadCurrentRecoveryQuestion() {
  try {
    const result = await authApiFetch('/auth/recovery-question');
    currentRecoveryQuestionText.textContent = result.available
      ? t('currentRecoveryQuestionPrefix', { question: result.question })
      : t('noRecoveryQuestionYet');
    currentRecoveryQuestionText.classList.remove('hidden');
  } catch (error) {
    currentRecoveryQuestionText.classList.add('hidden');
  }
}

async function handleSetRecoveryQuestion(event) {
  event.preventDefault();

  const currentPassword = recoveryCurrentPasswordInput.value;
  const securityQuestion = recoveryQuestionInput.value.trim();
  const securityAnswer = recoveryAnswerInput.value;

  if (!currentPassword) {
    setRecoveryQuestionMessage(t('pleaseEnterCurrentPassword'), 'error');
    return;
  }
  if (!securityQuestion) {
    setRecoveryQuestionMessage(t('authSecurityQuestionRequired'), 'error');
    return;
  }
  if (!securityAnswer.trim()) {
    setRecoveryQuestionMessage(t('authSecurityAnswerRequired'), 'error');
    return;
  }

  saveRecoveryQuestionBtn.disabled = true;
  try {
    await apiFetch('/auth/set-recovery-question', {
      method: 'POST',
      body: JSON.stringify({ currentPassword, securityQuestion, securityAnswer }),
    });
    recoveryQuestionForm.reset();
    setRecoveryQuestionMessage(t('recoveryQuestionSaved'), 'success');
    loadCurrentRecoveryQuestion();
  } catch (error) {
    setRecoveryQuestionMessage(error.message || t('unableToSaveRecoveryQuestion'), 'error');
  } finally {
    saveRecoveryQuestionBtn.disabled = false;
  }
}

function initializeEventListeners() {
  productForm.addEventListener('submit', handleProductSubmit);
  changePasswordForm.addEventListener('submit', handleChangePassword);
  recoveryQuestionForm.addEventListener('submit', handleSetRecoveryQuestion);
  addCustomerForm.addEventListener('submit', handleAddCustomer);
  paymentMethodInput.addEventListener('change', () => {
    updateCreditUi();
    renderPaymentSummary();
  });
  cancelEditBtn.addEventListener('click', () => {
    clearForm();
    setFormMessage(t('productFormReset'), 'success');
  });

  searchInput.addEventListener('input', renderSearchResults);
  // Plain text fields, not type="number": number fields change the value on arrow keys / mouse wheel (800 -> 799.99).
  [productPriceInput, productPurchasePriceInput, discountInput, taxInput, amountPaidInput].forEach((input) => {
    input.addEventListener('input', () => sanitizeMoneyInput(input));
    input.addEventListener('blur', () => normalizeMoneyInput(input));
  });
  categoryFilterInput.addEventListener('change', renderInventoryTable);
  stockFilterInput.addEventListener('change', renderInventoryTable);
  inventorySortInput.addEventListener('change', renderInventoryTable);
  quickScanInput.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') {
      event.preventDefault();
      addScannedProduct();
    }
  });
  shopNameInput.addEventListener('input', persistShopName);
  shopPhoneInput.addEventListener('input', persistShopName);
  shopAddressInput.addEventListener('input', persistShopName);
  designInput.addEventListener('change', () => {
    const design = applyDesign(designInput.value);
    try {
      localStorage.setItem(STORAGE_KEYS.design, design);
    } catch (error) {
      // The design still applies for this visit.
    }
  });

  printerSizeInput.addEventListener('change', () => {
    state.printerSize = printerSizeInput.value;
    localStorage.setItem(STORAGE_KEYS.printerSize, state.printerSize);
  });

  languageInput.addEventListener('change', () => {
    applyLanguage(languageInput.value);
    try {
      localStorage.setItem(I18N_STORAGE_KEY, getLanguage());
    } catch (error) {
      // The language still applies for this visit.
    }
    languageInput.value = getLanguage();
    refreshDynamicViews();
  });

  currencyInput.addEventListener('change', () => {
    applyCurrency(currencyInput.value);
    try {
      localStorage.setItem(CURRENCY_STORAGE_KEY, getCurrency());
    } catch (error) {
      // The currency still applies for this visit.
    }
    currencyInput.value = getCurrency();
    refreshDynamicViews();
  });

  periodButtons.forEach((button) => {
    button.addEventListener('click', () => {
      analysisPeriod = button.dataset.period;
      periodButtons.forEach((otherButton) => otherButton.classList.toggle('active', otherButton === button));
      renderSalesAnalysis();
    });
  });
  shopSettingsBtn.addEventListener('click', () => {
    const isOpen = !shopSettingsPanel.classList.contains('hidden');
    shopSettingsPanel.classList.toggle('hidden', isOpen);
    shopSettingsBtn.setAttribute('aria-expanded', String(!isOpen));
    if (!isOpen) loadCurrentRecoveryQuestion();
  });
  logoutBtn.addEventListener('click', async () => {
    try {
      await apiFetch('/auth/logout', { method: 'POST' });
    } catch (error) {
      // Logging out client-side still matters even if the request itself failed.
    }
    setAuthToken(null);
    shopSettingsPanel.classList.add('hidden');
    shopSettingsBtn.setAttribute('aria-expanded', 'false');
    showAuthOverlay('login');
  });
  document.addEventListener('click', (event) => {
    if (shopSettingsPanel.classList.contains('hidden')) return;
    if (shopSettingsPanel.contains(event.target) || shopSettingsBtn.contains(event.target)) return;
    shopSettingsPanel.classList.add('hidden');
    shopSettingsBtn.setAttribute('aria-expanded', 'false');
  });
  generateBillBtn.addEventListener('click', generateBill);
  document.getElementById('exportReceiptBtn')?.addEventListener('click', exportReceiptFile);
  document.getElementById('exportSalesCsvBtn')?.addEventListener('click', exportSalesCsv);
  newBillBtn.addEventListener('click', startNewBill);
  startNewDayBtn?.addEventListener('click', startNewDay);
  backupNowBtn?.addEventListener('click', backupNow);
  resetMonthlyProfitBtn?.addEventListener('click', resetMonthlyProfit);
  [discountInput, taxInput, amountPaidInput].forEach((input) => {
    input.addEventListener('input', renderPaymentSummary);
  });
  billItemsList.addEventListener('change', (event) => {
    const target = event.target;
    if (!(target instanceof HTMLInputElement) || target.dataset.action !== 'set-bill-quantity') return;
    setBillQuantity(target.dataset.id, target.value);
  });

  document.addEventListener('keydown', (event) => {
    // Billing shortcuts open the Current Bill tab first, since their fields live there.
    if (event.ctrlKey && event.key.toLowerCase() === 'k') {
      event.preventDefault();
      showTab('bill');
      searchInput.focus();
      searchInput.select();
    }

    if (event.ctrlKey && event.key === 'Enter') {
      event.preventDefault();
      showTab('bill');
      generateBill();
    }

    if (event.key === 'F2') {
      event.preventDefault();
      showTab('bill');
      quickScanInput.focus();
      quickScanInput.select();
    }
  });

  document.addEventListener('click', (event) => {
    const target = event.target;
    if (!(target instanceof HTMLElement)) return;

    const tabTrigger = target.closest('[data-tab-target]');
    if (tabTrigger) {
      showTab(tabTrigger.dataset.tabTarget);
      return;
    }

    const action = target.dataset.action;
    const productId = target.dataset.id;

    // The receipt buttons carry no data-id, so handle them before the id check below.
    if (action === 'print-receipt') {
      printReceipt();
      return;
    }

    if (action === 'download-receipt') {
      exportReceiptFile();
      return;
    }

    if (action === 'cancel-bill-edit') {
      cancelBillEdit();
      return;
    }

    if (!action || !productId) return;

    if (action === 'edit-product') {
      populateEditForm(productId);
    }

    if (action === 'delete-product') {
      deleteProduct(productId);
    }

    if (action === 'add-search-product') {
      addToBill(productId, 1);
    }

    if (action === 'increase-bill-item') {
      increaseBillItem(productId);
    }

    if (action === 'decrease-bill-item') {
      decreaseBillItem(productId);
    }

    if (action === 'remove-bill-item') {
      removeBillItem(productId);
    }

    if (action === 'view-sale') {
      const sale = state.sales.find((savedSale) => savedSale.id === productId);
      if (sale) {
        showReceipt(sale);
        showTab('bill');
        receiptContainer.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      }
    }

    if (action === 'add-items-to-sale') {
      startAddItemsToSale(productId);
    }

    if (action === 'void-sale') {
      voidSale(productId);
    }

    if (action === 'record-customer-payment') {
      recordCustomerPayment(productId);
    }

    if (action === 'edit-customer') {
      editCustomer(productId);
    }
  });
}

// A logout followed by logging back in calls this again without a page reload, so the
// one-time setup (listeners, the refresh timer) is guarded separately from the data
// reload, which should happen on every successful login.
let appBooted = false;

async function bootApp() {
  if (!appBooted) {
    appBooted = true;
    initializeEventListeners();
    window.setInterval(() => {
      initializeBusinessDay();
      initializeMonthlyProfit();
      updateShopDisplay();
      renderDashboardStats();
    }, 60000);
  }

  showTab(getSavedTab());
  initializeBusinessDay();
  initializeMonthlyProfit();
  await readProductsFromStorage();
  await loadShopName();
  await loadSales();
  await loadCustomers();
  renderInventoryTable();
  renderSearchResults();
  renderBill();
  renderCustomersList();
  populateCreditCustomerSelect();
  renderDashboardStats();
}

function setAuthFormMessage(message, type = '') {
  authFormMessage.textContent = message;
  authFormMessage.className = 'form-message';
  if (type) authFormMessage.classList.add(type);
}

let authMode = 'login';
// Fetched from the public /auth/recovery-question endpoint right before switching into
// 'recover' mode, then just displayed by showAuthOverlay - see enterRecoveryMode().
let recoveryQuestionText = '';

// mode: 'login' once a username/password already exists, 'setup' the very first time
// the software runs (no auth.txt yet), 'recover' after "Forgot password?" is clicked and
// a security question was found. message is shown inline, e.g. a stale-session or
// offline-backend notice, so the reason for landing back here is never a mystery.
function showAuthOverlay(mode, message) {
  authMode = mode;
  const isSetup = mode === 'setup';
  const isRecover = mode === 'recover';

  appShell.classList.add('hidden');
  authOverlay.classList.remove('hidden');
  authForm.reset();

  authHeading.textContent = isSetup ? t('authSetupHeading') : isRecover ? t('authRecoverHeading') : t('authLoginHeading');
  authSubtitle.textContent = isSetup ? t('authSetupSubtitle') : isRecover ? t('authRecoverSubtitle') : t('authLoginSubtitle');

  authUsernameGroup.classList.toggle('hidden', isRecover);
  authSecurityQuestionSetGroup.classList.toggle('hidden', !isSetup);
  authRecoveryQuestionText.classList.toggle('hidden', !isRecover);
  authRecoveryQuestionText.textContent = isRecover ? recoveryQuestionText : '';
  authSecurityAnswerGroup.classList.toggle('hidden', !(isSetup || isRecover));
  authSecurityAnswerLabel.textContent = isSetup ? t('authSecurityAnswerLabel') : t('authYourAnswerLabel');

  authPasswordLabel.textContent = isRecover ? t('authNewPasswordLabel') : t('authPasswordLabel');
  authPasswordInput.placeholder = isRecover ? t('authNewPasswordPlaceholder') : t('authPasswordPlaceholder');
  authConfirmGroup.classList.toggle('hidden', !(isSetup || isRecover));
  authConfirmPasswordLabel.textContent = isRecover ? t('authConfirmNewPasswordLabel') : t('authConfirmPasswordLabel');

  authSubmitBtn.textContent = isSetup ? t('authCreateLoginBtn') : isRecover ? t('authResetPasswordBtn') : t('authSignInBtn');
  authForgotPasswordBtn.classList.toggle('hidden', mode !== 'login');
  authBackToLoginBtn.classList.toggle('hidden', !isRecover);

  setAuthFormMessage(message || '', message ? 'error' : '');
  (isRecover ? authSecurityAnswerInput : authUsernameInput).focus();
}

function hideAuthOverlay() {
  authOverlay.classList.add('hidden');
  appShell.classList.remove('hidden');
}

// Looks up whether this shop set a recovery question at signup; shops set up before this
// feature existed have none, so that's reported back as plainly "not available" rather
// than an error - there's nothing broken, just nothing to recover with yet.
async function enterRecoveryMode() {
  authForgotPasswordBtn.disabled = true;
  try {
    const result = await authApiFetch('/auth/recovery-question');
    if (!result.available) {
      setAuthFormMessage(t('authNoRecoverySet'), 'error');
      return;
    }
    recoveryQuestionText = result.question;
    showAuthOverlay('recover');
  } catch (error) {
    setAuthFormMessage(error.message || t('authRecoveryUnavailable'), 'error');
  } finally {
    authForgotPasswordBtn.disabled = false;
  }
}

async function handleAuthSubmit(event) {
  event.preventDefault();

  if (authMode === 'recover') {
    const answer = authSecurityAnswerInput.value;
    const newPassword = authPasswordInput.value;

    if (!answer.trim()) {
      setAuthFormMessage(t('authAnswerRequired'), 'error');
      return;
    }
    if (newPassword.length < 4) {
      setAuthFormMessage(t('authPasswordTooShort'), 'error');
      return;
    }
    if (newPassword !== authConfirmPasswordInput.value) {
      setAuthFormMessage(t('authPasswordMismatch'), 'error');
      return;
    }

    authSubmitBtn.disabled = true;
    try {
      const result = await authApiFetch('/auth/recover', {
        method: 'POST',
        body: JSON.stringify({ answer, newPassword }),
      });
      setAuthToken(result.token);
      hideAuthOverlay();
      await bootApp();
    } catch (error) {
      setAuthFormMessage(error.status === 401 ? t('authWrongAnswer') : (error.message || t('authLoginFailed')), 'error');
    } finally {
      authSubmitBtn.disabled = false;
    }
    return;
  }

  const username = authUsernameInput.value.trim();
  const password = authPasswordInput.value;

  if (!username) {
    setAuthFormMessage(t('authUsernameRequired'), 'error');
    return;
  }

  let securityQuestion = '';
  let securityAnswer = '';
  if (authMode === 'setup') {
    if (password.length < 4) {
      setAuthFormMessage(t('authPasswordTooShort'), 'error');
      return;
    }
    if (password !== authConfirmPasswordInput.value) {
      setAuthFormMessage(t('authPasswordMismatch'), 'error');
      return;
    }
    securityQuestion = authSecurityQuestionInput.value.trim();
    securityAnswer = authSecurityAnswerInput.value;
    if (!securityQuestion) {
      setAuthFormMessage(t('authSecurityQuestionRequired'), 'error');
      return;
    }
    if (!securityAnswer.trim()) {
      setAuthFormMessage(t('authSecurityAnswerRequired'), 'error');
      return;
    }
  } else if (!password) {
    setAuthFormMessage(t('authPasswordRequired'), 'error');
    return;
  }

  authSubmitBtn.disabled = true;
  try {
    const endpoint = authMode === 'setup' ? '/auth/setup' : '/auth/login';
    const body = authMode === 'setup' ? { username, password, securityQuestion, securityAnswer } : { username, password };
    const result = await authApiFetch(endpoint, {
      method: 'POST',
      body: JSON.stringify(body),
    });
    setAuthToken(result.token);
    hideAuthOverlay();
    await bootApp();
  } catch (error) {
    setAuthFormMessage(error.status === 401 ? t('authInvalidCredentials') : (error.message || t('authLoginFailed')), 'error');
  } finally {
    authSubmitBtn.disabled = false;
  }
}

authForm.addEventListener('submit', handleAuthSubmit);
authForgotPasswordBtn.addEventListener('click', enterRecoveryMode);
authBackToLoginBtn.addEventListener('click', () => showAuthOverlay('login'));

async function init() {
  const token = getAuthToken();
  if (token) {
    hideAuthOverlay();
    await bootApp();
    return;
  }

  try {
    const status = await authApiFetch('/auth/status');
    showAuthOverlay(status.configured ? 'login' : 'setup');
  } catch (error) {
    // Backend unreachable: still show the login screen (the shop almost certainly
    // already has credentials), with the connection problem explained inline.
    showAuthOverlay('login', error.message);
  }
}

init();
