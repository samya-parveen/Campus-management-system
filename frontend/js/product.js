(() => {
  "use strict";

  const categories = ["Book", "Accessory", "Stationery"];
  const categoryIcons = { Book: "▤", Accessory: "◇", Stationery: "✎" };
  const table = document.getElementById("productTable");
  const form = document.getElementById("productForm");

  function renderProducts() {
    const products = window.CSMS.products.all();
    const search = (document.getElementById("searchInput")?.value || "").trim().toLowerCase();
    const category = document.getElementById("categoryFilter")?.value || "";
    const stockFilter = document.getElementById("stockFilter")?.value || "";
    const filtered = products.filter((product) => {
      const matchesSearch = product.name.toLowerCase().includes(search) || String(product.id).includes(search);
      const matchesCategory = !category || product.category === category;
      const matchesStock = !stockFilter || (stockFilter === "low" ? Number(product.quantity) <= 5 : Number(product.quantity) > 5);
      return matchesSearch && matchesCategory && matchesStock;
    });

    const total = document.getElementById("totalProducts");
    const categoryTotal = document.getElementById("totalCategories");
    const lowStock = document.getElementById("lowStockProducts");
    const inventoryValue = document.getElementById("inventoryValue");
    const visibleCount = document.getElementById("visibleProductCount");
    const emptyMessage = document.getElementById("emptyMessage");
    if (total) total.textContent = String(products.length);
    if (categoryTotal) categoryTotal.textContent = String(new Set(products.map((product) => product.category)).size);
    if (lowStock) lowStock.textContent = String(products.filter((product) => Number(product.quantity) <= 5).length);
    if (inventoryValue) inventoryValue.textContent = window.CSMS.formatMoney(products.reduce((sum, product) => sum + Number(product.basePrice ?? product.price) * Number(product.quantity), 0));
    if (visibleCount) visibleCount.textContent = String(filtered.length);
    if (emptyMessage) emptyMessage.hidden = filtered.length !== 0;
    if (!table) return;

    table.replaceChildren();
    filtered.forEach((product) => {
      const row = document.createElement("tr");
      const productCell = document.createElement("td");
      const productMain = document.createElement("div");
      productMain.className = "product-main";
      const icon = document.createElement("span");
      icon.className = "product-thumb";
      icon.textContent = categoryIcons[product.category] || "▦";
      const titleWrap = document.createElement("span");
      const title = document.createElement("span");
      title.className = "product-title";
      title.textContent = product.name;
      const id = document.createElement("small");
      id.className = "product-id";
      id.textContent = `ID ${product.id}`;
      titleWrap.append(title, id);
      productMain.append(icon, titleWrap);
      productCell.append(productMain);

      const categoryCell = document.createElement("td");
      const categoryTag = document.createElement("span");
      categoryTag.className = "category-tag";
      categoryTag.textContent = product.category;
      categoryCell.append(categoryTag);
      const priceCell = document.createElement("td");
      priceCell.className = "product-price";
      priceCell.textContent = window.CSMS.formatMoney(product.basePrice ?? product.price);
      const stockCell = document.createElement("td");
      const stockTag = document.createElement("span");
      const quantity = Number(product.quantity);
      stockTag.className = `stock-tag${quantity === 0 ? " out" : quantity <= 5 ? " low" : ""}`;
      stockTag.textContent = quantity === 0 ? "Out of stock" : `${quantity} in stock`;
      stockCell.append(stockTag);
      const actionsCell = document.createElement("td");
      actionsCell.className = "align-right";
      const actions = document.createElement("div");
      actions.className = "row-actions";
      const edit = document.createElement("a");
      edit.className = "row-action";
      edit.href = `add-products.html?id=${encodeURIComponent(product.id)}`;
      edit.textContent = "Edit";
      const remove = document.createElement("button");
      remove.type = "button";
      remove.className = "row-action delete";
      remove.textContent = "Delete";
      remove.setAttribute("aria-label", `Delete ${product.name}`);
      remove.addEventListener("click", async () => {
        if (!window.confirm(`Delete "${product.name}" from your products?`)) return;
        try {
          await window.CSMS.products.remove(product.id);
          renderProducts();
          window.CSMS.toast("Product deleted.");
        } catch (error) {
          window.CSMS.notifyError(error);
        }
      });
      actions.append(edit, remove);
      actionsCell.append(actions);
      row.append(productCell, categoryCell, priceCell, stockCell, actionsCell);
      table.append(row);
    });
  }

  function setupForm() {
    if (!form) return;
    const params = new URLSearchParams(window.location.search);
    const editingId = params.get("id");
    const idField = document.getElementById("productId");
    const nameField = document.getElementById("productName");
    const categoryField = document.getElementById("category");
    const priceField = document.getElementById("productPrice");
    const quantityField = document.getElementById("productQuantity");
    const error = document.getElementById("productFormError");
    if (editingId !== null) {
      const existing = window.CSMS.products.all().find((product) => Number(product.id) === Number(editingId));
      if (!existing) {
        window.CSMS.toast("That product could not be found.", "error");
        window.setTimeout(() => window.location.replace("products.html"), 700);
      } else {
        document.getElementById("pageTitle").textContent = "Edit product";
        idField.value = String(existing.id);
        idField.disabled = true;
        nameField.value = existing.name;
        categoryField.value = existing.category;
        priceField.value = String(existing.basePrice ?? existing.price);
        quantityField.value = String(existing.quantity);
      }
    }

    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      const id = Number(idField.value);
      const name = nameField.value.trim();
      const price = Number(priceField.value);
      const quantity = Number(quantityField.value);
      if (!Number.isSafeInteger(id) || id <= 0 || !name || !categories.includes(categoryField.value) || !Number.isFinite(price) || price <= 0 || !Number.isSafeInteger(quantity) || quantity < 0) {
        error.textContent = "Enter a valid product ID, name, category, price, and whole-number quantity.";
        error.hidden = false;
        return;
      }
      const products = window.CSMS.products.all();
      const duplicate = products.some((product) => Number(product.id) === id && Number(product.id) !== Number(editingId));
      if (duplicate) {
        error.textContent = "That product ID is already in use. Choose a different ID.";
        error.hidden = false;
        idField.focus();
        return;
      }
      try {
        await window.CSMS.products.save({ id, name, category: categoryField.value, price, quantity });
        window.CSMS.toast(editingId === null ? "Product added to your inventory." : "Product details updated.");
        window.location.assign("products.html");
      } catch (saveError) {
        error.textContent = saveError.message;
        error.hidden = false;
      }
    });
  }

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
    if (table) {
      ["searchInput", "categoryFilter", "stockFilter"].forEach((id) => {
        document.getElementById(id)?.addEventListener("input", renderProducts);
        document.getElementById(id)?.addEventListener("change", renderProducts);
      });
      renderProducts();
    }
    setupForm();
  }).catch(window.CSMS.notifyError);
})();