(() => {
  "use strict";

  let sales = [];
  let products = [];
  const isDashboard = Boolean(document.getElementById("salesChart"));

  function getRangeSales(days) {
    const start = new Date();
    start.setHours(0, 0, 0, 0);
    if (days !== "all") start.setDate(start.getDate() - Number(days) + 1);
    return sales.filter((sale) => days === "all" || new Date(sale.date) >= start);
  }

  function renderDashboard() {
    const today = new Date();
    const todayKey = today.toDateString();
    const todaysSales = sales.filter((sale) => new Date(sale.date).toDateString() === todayKey);
    const revenue = todaysSales.reduce((sum, sale) => sum + Number(sale.total), 0);
    const units = products.reduce((sum, product) => sum + Number(product.quantity), 0);
    document.getElementById("todayRevenue").textContent = window.CSMS.formatMoney(revenue);
    document.getElementById("todaySalesCount").textContent = `${todaysSales.length} completed ${todaysSales.length === 1 ? "sale" : "sales"}`;
    document.getElementById("productCount").textContent = String(products.length);
    document.getElementById("inventoryUnits").textContent = `${units} units in stock`;
    document.getElementById("lowStockCount").textContent = String(products.filter((product) => Number(product.quantity) <= 5).length);
    document.getElementById("averageOrder").textContent = window.CSMS.formatMoney(todaysSales.length ? revenue / todaysSales.length : 0);
    const welcomeDate = document.getElementById("welcomeDate");
    welcomeDate.textContent = new Intl.DateTimeFormat("en-IN", { weekday: "long", day: "numeric", month: "long" }).format(today).toUpperCase();
    const recentBody = document.getElementById("recentSales");
    recentBody.replaceChildren();
    sales.slice(0, 5).forEach((sale) => {
      const row = document.createElement("tr");
      const id = document.createElement("td");
      id.className = "sale-id";
      id.textContent = sale.id;
      const items = document.createElement("td");
      items.className = "sale-items";
      const itemCount = sale.items.reduce((sum, item) => sum + Number(item.quantity), 0);
      items.textContent = `${itemCount} ${itemCount === 1 ? "item" : "items"}`;
      const date = document.createElement("td");
      date.textContent = new Intl.DateTimeFormat("en-IN", { day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" }).format(new Date(sale.date));
      const total = document.createElement("td");
      total.className = "sale-total align-right";
      total.textContent = window.CSMS.formatMoney(sale.total);
      row.append(id, items, date, total);
      recentBody.append(row);
    });
    if (sales.length === 0) recentBody.innerHTML = '<tr><td class="empty-state" colspan="4">Your completed sales will appear here.</td></tr>';
    renderChart(Number(document.getElementById("chartRange").value));
    renderPopularProducts();
  }

  function renderChart(days) {
    const chart = document.getElementById("salesChart");
    chart.replaceChildren();
    chart.style.gridTemplateColumns = `repeat(${days}, minmax(0, 1fr))`;
    const start = new Date();
    start.setHours(0, 0, 0, 0);
    start.setDate(start.getDate() - days + 1);
    const buckets = Array.from({ length: days }, (_, index) => {
      const date = new Date(start);
      date.setDate(start.getDate() + index);
      return {
        date,
        total: sales.filter((sale) => new Date(sale.date).toDateString() === date.toDateString()).reduce((sum, sale) => sum + Number(sale.total), 0)
      };
    });
    const max = Math.max(...buckets.map((bucket) => bucket.total), 1);
    buckets.forEach((bucket, index) => {
      const column = document.createElement("div");
      column.className = "chart-column";
      const value = document.createElement("span");
      value.className = "chart-value";
      value.textContent = bucket.total ? new Intl.NumberFormat("en-IN", { notation: "compact", maximumFractionDigits: 1 }).format(bucket.total) : "";
      const wrap = document.createElement("div");
      wrap.className = "chart-bar-wrap";
      const bar = document.createElement("div");
      bar.className = `chart-bar${index === buckets.length - 1 ? " today" : ""}`;
      bar.style.height = `${Math.max(bucket.total ? bucket.total / max * 100 : 2, 2)}%`;
      bar.title = `${window.CSMS.formatDate(bucket.date, { day: "numeric", month: "short" })}: ${window.CSMS.formatMoney(bucket.total)}`;
      wrap.append(bar);
      const label = document.createElement("span");
      label.className = "chart-label";
      label.textContent = new Intl.DateTimeFormat("en-IN", { weekday: days > 7 ? undefined : "short", day: days > 7 ? "numeric" : undefined }).format(bucket.date);
      column.append(value, wrap, label);
      chart.append(column);
    });
  }

  function renderPopularProducts() {
    const popular = document.getElementById("popularProducts");
    const quantities = new Map();
    sales.forEach((sale) => sale.items.forEach((item) => quantities.set(item.id, (quantities.get(item.id) || 0) + Number(item.quantity))));
    const ranked = [...quantities.entries()].sort((first, second) => second[1] - first[1]).slice(0, 4);
    popular.replaceChildren();
    if (ranked.length === 0) {
      const empty = document.createElement("p");
      empty.className = "muted";
      empty.textContent = "Complete a sale to see your popular products.";
      popular.append(empty);
      return;
    }
    ranked.forEach(([id, quantity], index) => {
      const product = products.find((item) => Number(item.id) === Number(id));
      const item = document.createElement("div");
      item.className = "popular-item";
      const rank = document.createElement("span");
      rank.className = "popular-rank";
      rank.textContent = String(index + 1).padStart(2, "0");
      const detail = document.createElement("span");
      const name = document.createElement("span");
      name.className = "popular-name";
      name.textContent = product ? product.name : "Removed product";
      const category = document.createElement("span");
      category.className = "popular-category";
      category.textContent = product ? product.category : "Past sale";
      detail.append(name, category);
      const sold = document.createElement("span");
      sold.className = "popular-qty";
      sold.textContent = `${quantity} sold`;
      item.append(rank, detail, sold);
      popular.append(item);
    });
  }

  function renderReports() {
    const range = document.getElementById("reportRange").value;
    const selected = getRangeSales(range);
    const revenue = selected.reduce((sum, sale) => sum + Number(sale.total), 0);
    document.getElementById("reportRevenue").textContent = window.CSMS.formatMoney(revenue);
    document.getElementById("reportOrders").textContent = String(selected.length);
    document.getElementById("reportAverage").textContent = window.CSMS.formatMoney(selected.length ? revenue / selected.length : 0);
    document.getElementById("reportPeriodLabel").textContent = range === "all" ? "All recorded sales" : `Last ${range} days`;
    document.getElementById("salesCountLabel").textContent = `${selected.length} ${selected.length === 1 ? "sale" : "sales"}`;
    const body = document.getElementById("reportSales");
    body.replaceChildren();
    selected.forEach((sale) => {
      const row = document.createElement("tr");
      const itemCount = sale.items.reduce((sum, item) => sum + Number(item.quantity), 0);
      const values = [
        { value: sale.id, className: "sale-id" },
        { value: `${itemCount} ${itemCount === 1 ? "item" : "items"}` },
        { value: sale.payment },
        { value: new Intl.DateTimeFormat("en-IN", { day: "numeric", month: "short", year: "numeric", hour: "2-digit", minute: "2-digit" }).format(new Date(sale.date)) },
        { value: window.CSMS.formatMoney(sale.total), className: "sale-total align-right" }
      ];
      values.forEach((entry) => {
        const cell = document.createElement("td");
        cell.textContent = entry.value;
        if (entry.className) cell.className = entry.className;
        row.append(cell);
      });
      body.append(row);
    });
    if (selected.length === 0) body.innerHTML = '<tr><td class="empty-state" colspan="5">No sales recorded in this period yet.</td></tr>';
    renderCategories();
    renderPayments(selected);
  }

  function renderCategories() {
    const breakdown = document.getElementById("categoryBreakdown");
    const categories = ["Book", "Accessory", "Stationery"];
    const counts = categories.map((category) => products.filter((product) => product.category === category).reduce((sum, product) => sum + Number(product.quantity), 0));
    const max = Math.max(...counts, 1);
    breakdown.replaceChildren();
    categories.forEach((category, index) => {
      const row = document.createElement("div");
      row.className = "category-row";
      const label = document.createElement("span");
      label.className = "category-label";
      label.textContent = { Book: "Books", Accessory: "Accessories", Stationery: "Stationery" }[category];
      const track = document.createElement("span");
      track.className = "category-track";
      const fill = document.createElement("span");
      fill.className = "category-fill";
      fill.style.display = "block";
      fill.style.width = `${counts[index] / max * 100}%`;
      track.append(fill);
      const amount = document.createElement("span");
      amount.className = "category-value";
      amount.textContent = `${counts[index]} units`;
      row.append(label, track, amount);
      breakdown.append(row);
    });
  }

  function renderPayments(selected) {
    const breakdown = document.getElementById("paymentBreakdown");
    const methods = [...new Set(["UPI", "Cash", "Card", ...selected.map((sale) => sale.payment)])];
    breakdown.replaceChildren();
    methods.forEach((method) => {
      const total = selected.filter((sale) => sale.payment === method).reduce((sum, sale) => sum + Number(sale.total), 0);
      const row = document.createElement("div");
      row.className = "payment-row";
      const label = document.createElement("span");
      label.className = "payment-method";
      const dot = document.createElement("span");
      dot.className = "payment-dot";
      label.append(dot, document.createTextNode(method));
      const amount = document.createElement("span");
      amount.className = "payment-amount";
      amount.textContent = window.CSMS.formatMoney(total);
      row.append(label, amount);
      breakdown.append(row);
    });
  }

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
    sales = window.CSMS.sales.all().sort((first, second) => new Date(second.date) - new Date(first.date));
    products = window.CSMS.products.all();
    if (isDashboard) {
      renderDashboard();
      document.getElementById("chartRange").addEventListener("change", (event) => renderChart(Number(event.target.value)));
    }
    if (document.getElementById("reportRange")) {
      renderReports();
      document.getElementById("reportRange").addEventListener("change", renderReports);
    }
  }).catch(window.CSMS.notifyError);
})();