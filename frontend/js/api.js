(() => {
  "use strict";

  const TOKEN_KEY = "csmsApiToken";
  const USER_KEY = "csmsUser";
  const state = { products: [], sales: [], coupons: [], discounts: [], audit: [] };
  const cartKey = "csmsAdminCart";
  const customerCartKey = "csmsCustomerCart";

  function readUser() {
    try {
      return JSON.parse(localStorage.getItem(USER_KEY) || "null");
    } catch {
      localStorage.removeItem(USER_KEY);
      return null;
    }
  }

  function saveUser(user) {
    if (user) localStorage.setItem(USER_KEY, JSON.stringify(user));
    else localStorage.removeItem(USER_KEY);
  }

  function clearSession() {
    localStorage.removeItem(TOKEN_KEY);
    saveUser(null);
  }

  async function request(path, options = {}) {
    const headers = new Headers(options.headers || {});
    if (options.body !== undefined) headers.set("Content-Type", "application/json");
    const token = localStorage.getItem(TOKEN_KEY);
    if (token) headers.set("Authorization", `Bearer ${token}`);
    const response = await fetch(`/api${path}`, {
      ...options,
      headers,
      body: options.body === undefined ? undefined : JSON.stringify(options.body)
    });
    const contentType = response.headers.get("content-type") || "";
    const result = contentType.includes("application/json") ? await response.json() : null;
    if (!response.ok) {
      if (response.status === 401 && path !== "/auth/login" && path !== "/auth/register") clearSession();
      throw new Error(result?.error || `The server returned ${response.status}.`);
    }
    return result;
  }

  async function loadData(user) {
    const [products, sales, coupons, discounts] = await Promise.all([
      request("/products"),
      request("/sales"),
      request("/coupons"),
      request("/discounts")
    ]);
    state.products = products.items;
    state.sales = sales.items;
    state.coupons = coupons.items;
    state.discounts = discounts.items;
    state.audit = user.role === "admin" ? (await request("/audit")).items : [];
  }

  function saveLogin(response) {
    localStorage.setItem(TOKEN_KEY, response.token);
    saveUser(response.user);
    return response.user;
  }

  function notifyError(error) {
    toast(error instanceof Error ? error.message : String(error), "error");
  }

  function formatMoney(value) {
    return new Intl.NumberFormat("en-IN", {
      style: "currency",
      currency: "INR",
      maximumFractionDigits: 2
    }).format(Number(value) || 0);
  }

  function formatDate(value, options = { day: "numeric", month: "short" }) {
    return new Intl.DateTimeFormat("en-IN", options).format(new Date(value));
  }

  function toast(message, type = "success") {
    const existing = document.querySelector(".toast");
    if (existing) existing.remove();
    const notification = document.createElement("div");
    notification.className = `toast${type === "error" ? " error" : ""}`;
    notification.setAttribute("role", "status");
    notification.textContent = message;
    document.body.append(notification);
    window.setTimeout(() => notification.remove(), 3200);
  }

  window.CSMS = {
    ready: Promise.resolve(false),
    request,
    loadData,
    notifyError,
    products: {
      all: () => state.products,
      save: async (product) => {
        const existing = state.products.some((item) => Number(item.id) === Number(product.id));
        const result = await request(existing ? `/products/${product.id}` : "/products", {
          method: existing ? "PUT" : "POST",
          body: { ...product, basePrice: Number(product.basePrice ?? product.price) }
        });
        const index = state.products.findIndex((item) => Number(item.id) === Number(result.id));
        if (index < 0) state.products.push(result);
        else state.products[index] = result;
        return result;
      },
      remove: async (id) => {
        await request(`/products/${encodeURIComponent(id)}`, { method: "DELETE" });
        state.products = state.products.filter((item) => Number(item.id) !== Number(id));
      }
    },
    sales: {
      all: () => state.sales,
      add: async (sale) => {
        const result = await request("/sales", { method: "POST", body: sale });
        state.sales.unshift(result);
        await loadData(readUser());
        return result;
      },
      byId: (id) => request(`/sales/${encodeURIComponent(id)}`)
    },
    coupons: {
      all: () => state.coupons,
      featured: () => state.coupons.find((coupon) =>
        coupon.active &&
        (!coupon.expiresAt || new Date(`${coupon.expiresAt}T23:59:59`) >= new Date()) &&
        (!Number(coupon.maxUses) || Number(coupon.usageCount) < Number(coupon.maxUses))
      ) || null,
      save: async (coupon) => {
        const existing = state.coupons.some((item) => item.code === coupon.code);
        const result = await request(existing ? `/coupons/${encodeURIComponent(coupon.code)}` : "/coupons", {
          method: existing ? "PUT" : "POST",
          body: coupon
        });
        const index = state.coupons.findIndex((item) => item.code === result.code);
        if (index < 0) state.coupons.push(result);
        else state.coupons[index] = result;
        return result;
      },
      remove: async (code) => {
        await request(`/coupons/${encodeURIComponent(code)}`, { method: "DELETE" });
        state.coupons = state.coupons.filter((item) => item.code !== code);
      }
    },
    discounts: {
      all: () => state.discounts,
      save: async (discount) => {
        const existing = state.discounts.some((item) => item.id === discount.id);
        const result = await request(existing ? `/discounts/${encodeURIComponent(discount.id)}` : "/discounts", {
          method: existing ? "PUT" : "POST",
          body: discount
        });
        const index = state.discounts.findIndex((item) => item.id === result.id);
        if (index < 0) state.discounts.push(result);
        else state.discounts[index] = result;
        return result;
      },
      remove: async (id) => {
        await request(`/discounts/${encodeURIComponent(id)}`, { method: "DELETE" });
        state.discounts = state.discounts.filter((item) => item.id !== id);
      }
    },
    calculateDiscount: async (items, couponCode = "") => {
      const requestItems = items.map((item) => ({ id: Number(item.id), quantity: Number(item.quantity) }));
      return request("/pricing/quote", {
        method: "POST",
        body: { items: requestItems, couponCode }
      });
    },
    auth: {
      current: readUser,
      hasToken: () => Boolean(localStorage.getItem(TOKEN_KEY)),
      clear: clearSession,
      me: async () => {
        const result = await request("/auth/me");
        saveUser(result.user);
        return result.user;
      },
      authenticate: async (email, password) =>
        saveLogin(await request("/auth/login", { method: "POST", body: { email, password } })),
      register: async (profile, password) => {
        const result = await request("/auth/register", {
          method: "POST",
          body: { ...profile, password }
        });
        saveLogin(result);
        return { user: result.user };
      },
      updateProfile: async (profile) => {
        const result = await request("/profile", { method: "PUT", body: profile });
        saveUser(result.user);
        return result.user;
      },
      changePassword: (currentPassword, newPassword) =>
        request("/profile/password", { method: "PUT", body: { currentPassword, newPassword } }),
      signOut: async () => {
        try {
          if (localStorage.getItem(TOKEN_KEY)) {
            await request("/auth/logout", { method: "POST" });
          }
        } finally {
          clearSession();
        }
      }
    },
    audit: { all: () => state.audit },
    cartKey,
    customerCartKey,
    formatMoney,
    formatDate,
    toast
  };
})();
