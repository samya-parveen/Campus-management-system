PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
  email TEXT PRIMARY KEY COLLATE NOCASE,
  name TEXT NOT NULL,
  phone TEXT NOT NULL DEFAULT '',
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('admin', 'customer')),
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
  token_hash TEXT PRIMARY KEY,
  email TEXT NOT NULL REFERENCES users(email) ON DELETE CASCADE,
  expires_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS products (
  id INTEGER PRIMARY KEY CHECK (id > 0),
  name TEXT NOT NULL,
  category TEXT NOT NULL CHECK (category IN ('Book', 'Accessory', 'Stationery')),
  base_price REAL NOT NULL CHECK (base_price >= 0),
  quantity INTEGER NOT NULL CHECK (quantity >= 0),
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS coupons (
  code TEXT PRIMARY KEY COLLATE NOCASE,
  type TEXT NOT NULL CHECK (type IN ('percentage', 'fixed')),
  value REAL NOT NULL CHECK (value >= 0),
  category TEXT NOT NULL DEFAULT 'All',
  minimum_purchase REAL NOT NULL DEFAULT 0,
  max_uses INTEGER NOT NULL DEFAULT 0,
  expires_at TEXT NOT NULL DEFAULT '',
  active INTEGER NOT NULL DEFAULT 1,
  usage_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS discounts (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  category TEXT NOT NULL DEFAULT 'All',
  value REAL NOT NULL CHECK (value >= 0 AND value <= 100),
  starts_at TEXT NOT NULL DEFAULT '',
  ends_at TEXT NOT NULL DEFAULT '',
  active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS sales (
  id TEXT PRIMARY KEY,
  date TEXT NOT NULL,
  subtotal REAL NOT NULL,
  discount REAL NOT NULL,
  promotion_discount REAL NOT NULL,
  coupon_discount REAL NOT NULL,
  total REAL NOT NULL,
  payment TEXT NOT NULL,
  coupon_code TEXT NOT NULL DEFAULT '',
  channel TEXT NOT NULL CHECK (channel IN ('customer', 'admin')),
  customer_name TEXT NOT NULL,
  customer_email TEXT NOT NULL,
  fulfillment TEXT NOT NULL DEFAULT '',
  delivery_address TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS sale_items (
  sale_id TEXT NOT NULL REFERENCES sales(id) ON DELETE CASCADE,
  product_id INTEGER NOT NULL,
  name TEXT NOT NULL,
  category TEXT NOT NULL,
  price REAL NOT NULL,
  quantity INTEGER NOT NULL CHECK (quantity > 0),
  line_subtotal REAL NOT NULL,
  promotion_discount REAL NOT NULL,
  promotion_name TEXT NOT NULL DEFAULT '',
  PRIMARY KEY (sale_id, product_id)
);

CREATE TABLE IF NOT EXISTS audit_logs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  date TEXT NOT NULL,
  user TEXT NOT NULL,
  email TEXT NOT NULL DEFAULT '',
  role TEXT NOT NULL,
  action TEXT NOT NULL,
  details TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sales_date ON sales(date);
CREATE INDEX IF NOT EXISTS idx_audit_date ON audit_logs(date);
CREATE INDEX IF NOT EXISTS idx_sessions_expiry ON sessions(expires_at);