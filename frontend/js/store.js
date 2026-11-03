(() => {
  "use strict";

  const cartKey = window.CSMS.customerCartKey;
  let cart = JSON.parse(sessionStorage.getItem(cartKey) || "[]");
  let selectedCategory = "All";
  let couponCode = "";

  function saveCart() {
    sessionStorage.setItem(cartKey, JSON.stringify(cart));
    const count = cart.reduce((total, item) => total + Number(item.quantity), 0);
    ["customerCartCount", "checkoutCartCount"].forEach((id) => {
      const badge = document.getElementById(id);
      if (badge) badge.textContent = `${count}${id === "checkoutCartCount" ? ` ${count === 1 ? "item" : "items"}` : ""}`;
    });
  }

  function couponSavingsLabel(coupon) {
    return coupon.type === "fixed" ? window.CSMS.formatMoney(coupon.value) : `${coupon.value}%`;
  }

  function featuredCouponMessage() {
    const coupon = window.CSMS.coupons.featured();
    return coupon ? `Try ${coupon.code} for ${couponSavingsLabel(coupon)} off.` : "No coupon codes are available right now. Product offers still apply.";
  }

  function renderStoreCouponOffer() {
    const offer = document.getElementById("storeCouponOffer");
    if (!offer) return;
    const coupon = window.CSMS.coupons.featured();
    offer.hidden = !coupon;
    if (!coupon) return;
    const discount = coupon.type === "fixed" ? `Save ${couponSavingsLabel(coupon)}` : `Take ${couponSavingsLabel(coupon)} off`;
    document.getElementById("storeCouponOfferText").textContent = `${discount}${Number(coupon.minimumPurchase) > 0 ? ` orders over ${window.CSMS.formatMoney(coupon.minimumPurchase)}` : ""}`;
    document.getElementById("storeCouponCode").textContent = coupon.code;
  }

  function addToCart(id) {
    const product = window.CSMS.products.all().find((item) => Number(item.id) === Number(id));
    if (!product || Number(product.quantity) <= 0) {
      window.CSMS.toast("That item is no longer available.", "error");
      renderProducts().catch(window.CSMS.notifyError);
      return;
    }
    const existing = cart.find((item) => Number(item.id) === Number(id));
    if (existing && existing.quantity >= Number(product.quantity)) {
      window.CSMS.toast(`Only ${product.quantity} units are available.`, "error");
      return;
    }
    if (existing) existing.quantity += 1;
    else cart.push({ id: product.id, name: product.name, category: product.category, price: Number(product.price), quantity: 1 });
    saveCart();
    window.CSMS.toast(`${product.name} added to your basket.`);
    renderProducts().catch(window.CSMS.notifyError);
  }

  function refreshCustomerCart() {
    renderCustomerCart().catch(window.CSMS.notifyError);
  }

  async function renderProducts() {
    const grid = document.getElementById("storeProducts");
    if (!grid) return;
    const search = document.getElementById("storeSearch").value.trim().toLowerCase();
    const products = window.CSMS.products.all().filter((product) =>
      Number(product.quantity) > 0 &&
      (selectedCategory === "All" || product.category === selectedCategory) &&
      (!search || product.name.toLowerCase().includes(search) || String(product.id).includes(search))
    );
    grid.replaceChildren();
    document.getElementById("storeEmpty").hidden = products.length > 0;
    for (const [index, product] of products.entries()) {
      const productDiscount = await window.CSMS.calculateDiscount([{ id: product.id, quantity: 1 }]);
      const card = document.createElement("article");
      card.className = "store-product-card";
      const art = document.createElement("div");
      art.className = `product-art product-art-${product.category.toLowerCase()}`;
      const artLabel = document.createElement("span");
      artLabel.className = "product-art-label";
      artLabel.textContent = { Book: "CAMPUS READS", Accessory: "CAMPUS GOODS", Stationery: "STUDY ESSENTIALS" }[product.category] || "CAMPUS STORE";
      const artIcon = document.createElement("span");
      artIcon.className = "product-art-icon";
      artIcon.textContent = { Book: "▤", Accessory: "◇", Stationery: "✎" }[product.category] || "✦";
      art.append(artLabel, artIcon);
      const badge = document.createElement("span");
      badge.className = "product-art-count";
      badge.textContent = `0${index + 1}`;
      art.append(badge);
      const info = document.createElement("div");
      info.className = "store-product-info";
      const category = document.createElement("span");
      category.className = "store-product-category";
      category.textContent = product.category;
      const name = document.createElement("h3");
      name.textContent = product.name;
      const price = document.createElement("div");
      price.className = "store-product-price";
      if (productDiscount.promotionDiscount) {
        const sale = document.createElement("strong");
        sale.textContent = window.CSMS.formatMoney(Number(product.price) - productDiscount.promotionDiscount);
        const original = document.createElement("del");
        original.textContent = window.CSMS.formatMoney(product.price);
        price.append(sale, original);
        const offer = document.createElement("span");
        offer.className = "product-offer-badge";
        offer.textContent = `${Math.round(productDiscount.promotionDiscount / Number(product.price) * 100)}% off`;
        art.append(offer);
      } else {
        const regular = document.createElement("strong");
        regular.textContent = window.CSMS.formatMoney(product.price);
        price.append(regular);
      }
      const button = document.createElement("button");
      button.className = "add-product-button";
      button.type = "button";
      button.setAttribute("aria-label", `Add ${product.name} to basket`);
      button.textContent = "＋";
      button.addEventListener("click", () => addToCart(product.id));
      info.append(category, name, price);
      card.append(art, info, button);
      grid.append(card);
    }
  }

  async function renderCustomerCart() {
    const list = document.getElementById("customerCartItems");
    if (!list) return;
    const currentProducts = window.CSMS.products.all();
    const totals = cart.length
      ? await window.CSMS.calculateDiscount(cart, couponCode)
      : { subtotal: 0, discount: 0, total: 0, couponError: "" };
    const lineQuotes = await Promise.all(cart.map((item) =>
      window.CSMS.calculateDiscount([{ id: item.id, quantity: 1 }])
    ));
    list.replaceChildren();
    cart = cart.filter((item) => currentProducts.some((product) => Number(product.id) === Number(item.id)));
    if (!cart.length) {
      const empty = document.createElement("div");
      empty.className = "customer-cart-empty";
      const heading = document.createElement("strong");
      heading.textContent = "Your basket is taking a little breather.";
      const copy = document.createElement("p");
      copy.textContent = "There are lots of good things in the store, though.";
      const link = document.createElement("a");
      link.className = "button button-secondary";
      link.href = "index.html";
      link.textContent = "Find something lovely";
      empty.append(heading, copy, link);
      list.append(empty);
      couponCode = "";
      const couponInput = document.getElementById("customerCoupon");
      if (couponInput) couponInput.value = "";
      setCustomerCouponMessage(featuredCouponMessage());
    } else {
      cart.forEach((item, index) => {
        const available = currentProducts.find((product) => Number(product.id) === Number(item.id));
        item.price = Number(available.price);
        item.category = available.category;
        const row = document.createElement("article");
        row.className = "customer-cart-item";
        const icon = document.createElement("span");
        icon.className = `customer-item-icon customer-item-${item.category.toLowerCase()}`;
        icon.textContent = { Book: "▤", Accessory: "◇", Stationery: "✎" }[item.category] || "✦";
        const details = document.createElement("div");
        details.className = "customer-item-details";
        const name = document.createElement("strong");
        name.textContent = item.name;
        const category = document.createElement("small");
        category.textContent = item.category;
        const price = document.createElement("span");
        price.className = "customer-item-price";
        price.textContent = window.CSMS.formatMoney(lineQuotes[index].total);
        details.append(name, category, price);
        const controls = document.createElement("div");
        controls.className = "qty-control";
        const decrease = document.createElement("button");
        decrease.className = "qty-button";
        decrease.type = "button";
        decrease.textContent = "−";
        decrease.setAttribute("aria-label", `Remove one ${item.name}`);
        decrease.addEventListener("click", () => updateCustomerQuantity(item.id, item.quantity - 1));
        const quantity = document.createElement("span");
        quantity.textContent = String(item.quantity);
        const increase = document.createElement("button");
        increase.className = "qty-button";
        increase.type = "button";
        increase.textContent = "+";
        increase.setAttribute("aria-label", `Add one ${item.name}`);
        increase.addEventListener("click", () => updateCustomerQuantity(item.id, item.quantity + 1));
        controls.append(decrease, quantity, increase);
        const remove = document.createElement("button");
        remove.className = "remove-item";
        remove.type = "button";
        remove.textContent = "×";
        remove.setAttribute("aria-label", `Remove ${item.name}`);
        remove.addEventListener("click", () => updateCustomerQuantity(item.id, 0));
        row.append(icon, details, controls, remove);
        list.append(row);
      });
    }
    if (couponCode && totals.couponError) {
      couponCode = "";
      setCustomerCouponMessage(totals.couponError, true);
    }
    document.getElementById("customerSubtotal").textContent = window.CSMS.formatMoney(totals.subtotal);
    document.getElementById("customerSavings").textContent = `−${window.CSMS.formatMoney(totals.discount)}`;
    document.getElementById("customerTotal").textContent = window.CSMS.formatMoney(totals.total);
    const button = document.getElementById("placeCustomerOrder");
    if (button) button.disabled = cart.length === 0;
    saveCart();
  }

  function updateCustomerQuantity(id, quantity) {
    if (quantity <= 0) cart = cart.filter((item) => Number(item.id) !== Number(id));
    else {
      const item = cart.find((entry) => Number(entry.id) === Number(id));
      const product = window.CSMS.products.all().find((entry) => Number(entry.id) === Number(id));
      if (!item || !product || quantity > Number(product.quantity)) {
        window.CSMS.toast(`Only ${product ? product.quantity : 0} units are available.`, "error");
        return;
      }
      item.quantity = quantity;
    }
    refreshCustomerCart();
  }

  function setCustomerCouponMessage(message, isError = false) {
    const node = document.getElementById("customerCouponMessage");
    if (!node) return;
    node.textContent = message;
    node.classList.toggle("error", isError);
  }

  function setupCheckout() {
    if (!document.getElementById("customerOrderForm")) return;
    const form = document.getElementById("customerOrderForm");
    const error = document.getElementById("customerOrderError");
    const user = window.CSMS.auth.current();
    const fulfillment = document.getElementById("fulfillmentMethod");
    const deliveryField = document.getElementById("deliveryAddressField");
    const deliveryAddress = document.getElementById("deliveryAddress");
    if (user) {
      document.getElementById("customerName").value = user.name;
      document.getElementById("customerEmail").value = user.email;
      document.getElementById("customerName").readOnly = true;
      document.getElementById("customerEmail").readOnly = true;
    }
    fulfillment.addEventListener("change", () => {
      const requiresAddress = fulfillment.value === "Campus delivery";
      deliveryField.hidden = !requiresAddress;
      deliveryAddress.required = requiresAddress;
    });
    document.getElementById("emptyCustomerCart").addEventListener("click", () => {
      cart = [];
      couponCode = "";
      refreshCustomerCart();
    });
    document.getElementById("applyCustomerCoupon").addEventListener("click", async () => {
      const candidate = document.getElementById("customerCoupon").value.trim().toUpperCase();
      if (!cart.length) {
        couponCode = "";
        setCustomerCouponMessage("Add items before using a coupon.", true);
        return;
      }
      if (!candidate) {
        couponCode = "";
        setCustomerCouponMessage("Enter a coupon code first.", true);
        refreshCustomerCart();
        return;
      }
      try {
        const totals = await window.CSMS.calculateDiscount(cart, candidate);
        if (totals.couponError) {
          couponCode = "";
          setCustomerCouponMessage(totals.couponError, true);
        } else {
          couponCode = candidate;
          setCustomerCouponMessage(`${candidate} applied. You saved ${window.CSMS.formatMoney(totals.discount)}.`);
        }
        refreshCustomerCart();
      } catch (error) {
        setCustomerCouponMessage(error.message, true);
      }
    });
    document.getElementById("customerCoupon").addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        document.getElementById("applyCustomerCoupon").click();
      }
    });
    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      const customer = {
        name: document.getElementById("customerName").value.trim(),
        email: document.getElementById("customerEmail").value.trim().toLowerCase()
      };
      if (!cart.length) {
        error.textContent = "Your basket is empty. Add something from the store before placing an order.";
        error.hidden = false;
        return;
      }
      if (!customer.name || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(customer.email)) {
        error.textContent = "Enter your name and a valid email address.";
        error.hidden = false;
        return;
      }
      const products = window.CSMS.products.all();
      const unavailable = cart.find((item) => {
        const product = products.find((entry) => Number(entry.id) === Number(item.id));
        return !product || Number(product.quantity) < Number(item.quantity);
      });
      if (unavailable) {
        error.textContent = `${unavailable.name} is no longer available in that quantity. Please update your basket.`;
        error.hidden = false;
        refreshCustomerCart();
        return;
      }
      let totals;
      try {
        totals = await window.CSMS.calculateDiscount(cart, couponCode);
      } catch (quoteError) {
        error.textContent = quoteError.message;
        error.hidden = false;
        return;
      }
      if (couponCode && totals.couponError) {
        couponCode = "";
        error.textContent = totals.couponError;
        error.hidden = false;
        refreshCustomerCart();
        return;
      }
      const address = fulfillment.value === "Campus delivery" ? deliveryAddress.value.trim() : "";
      const sale = {
        payment: document.getElementById("customerPayment").value,
        couponCode,
        channel: "customer",
        fulfillment: fulfillment.value,
        deliveryAddress: address,
        items: cart.map((item) => ({ id: item.id, quantity: item.quantity }))
      };
      const submit = form.querySelector('button[type="submit"]');
      submit.disabled = true;
      try {
        const created = await window.CSMS.sales.add(sale);
        cart = [];
        sessionStorage.removeItem(cartKey);
        window.location.assign(`order.html?order=${encodeURIComponent(created.id)}`);
      } catch (orderError) {
        error.textContent = orderError.message;
        error.hidden = false;
      } finally {
        submit.disabled = false;
      }
    });
    refreshCustomerCart();
  }

  async function setupOrderConfirmation() {
    const orderNumber = document.getElementById("orderNumber");
    if (!orderNumber) return;
    const id = new URLSearchParams(window.location.search).get("order");
    const user = window.CSMS.auth.current();
    let sale = null;
    try {
      sale = await window.CSMS.sales.byId(id);
    } catch (error) {
      window.CSMS.notifyError(error);
    }
    if (sale && (sale.channel !== "customer" ||
        String(sale.customer?.email || "").toLowerCase() !== String(user?.email || "").toLowerCase())) sale = null;
    if (!sale) {
      document.getElementById("customerOrderDetails").hidden = true;
      document.getElementById("missingOrder").hidden = false;
      return;
    }
    document.getElementById("orderCustomerName").textContent = sale.customer.name.split(/\s+/)[0];
    orderNumber.textContent = sale.id;
    document.getElementById("orderFulfillment").textContent = sale.fulfillment;
    document.getElementById("orderPayment").textContent = sale.payment;
    document.getElementById("orderTotal").textContent = window.CSMS.formatMoney(sale.total);
    document.getElementById("orderEmail").textContent = sale.customer.email;
    const delivery = document.getElementById("orderDeliveryNote");
    if (sale.deliveryAddress) {
      delivery.textContent = `Campus delivery location: ${sale.deliveryAddress}`;
      delivery.hidden = false;
    }
    const breakdown = document.getElementById("orderTotalBreakdown");
    const summaryRows = [
      ["Items subtotal", window.CSMS.formatMoney(sale.subtotal)],
      ["Product discounts", `−${window.CSMS.formatMoney(sale.promotionDiscount || 0)}`]
    ];
    if (sale.couponCode) summaryRows.push([`Coupon · ${sale.couponCode}`, `−${window.CSMS.formatMoney(sale.couponDiscount || 0)}`]);
    summaryRows.push(["Order total", window.CSMS.formatMoney(sale.total)]);
    summaryRows.forEach(([label, amount], index) => {
      const row = document.createElement("div");
      row.className = `order-summary-row${index === summaryRows.length - 1 ? " final" : ""}`;
      const title = document.createElement("span");
      title.textContent = label;
      const value = document.createElement("strong");
      value.textContent = amount;
      row.append(title, value);
      breakdown.append(row);
    });
    const items = document.getElementById("orderItems");
    sale.items.forEach((item) => {
      const row = document.createElement("div");
      row.className = "order-item-row";
      const label = document.createElement("span");
      label.textContent = `${item.name} × ${item.quantity}`;
      const amount = document.createElement("strong");
      amount.textContent = window.CSMS.formatMoney(item.lineSubtotal - item.promotionDiscount);
      row.append(label, amount);
      items.append(row);
    });
    document.getElementById("copyOrderNumber").addEventListener("click", async (event) => {
      try {
        await navigator.clipboard.writeText(sale.id);
        event.currentTarget.textContent = "Copied";
      } catch {
        window.CSMS.toast(`Your order number is ${sale.id}.`);
      }
    });
  }

  function setupOrderLookup() {
    const form = document.getElementById("lookupOrdersForm");
    if (!form) return;
    const user = window.CSMS.auth.current();
    const emailField = document.getElementById("lookupEmail");
    if (user) {
      emailField.value = user.email;
      emailField.readOnly = true;
    }
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      const email = (user ? user.email : emailField.value).trim().toLowerCase();
      const results = document.getElementById("customerOrders");
      const matches = window.CSMS.sales.all()
        .filter((sale) => sale.channel === "customer" && sale.customer?.email.toLowerCase() === email)
        .sort((first, second) => new Date(second.date) - new Date(first.date));
      results.replaceChildren();
      const heading = document.createElement("h2");
      heading.className = "order-results-heading";
      heading.textContent = matches.length ? `Your orders (${matches.length})` : "No orders found for that email.";
      results.append(heading);
      matches.forEach((sale) => {
        const card = document.createElement("article");
        card.className = "customer-order-card";
        const details = document.createElement("div");
        const id = document.createElement("strong");
        id.textContent = sale.id;
        const meta = document.createElement("small");
        meta.textContent = `${window.CSMS.formatDate(sale.date, { day: "numeric", month: "short", year: "numeric" })} · ${sale.fulfillment} · ${sale.items.reduce((sum, item) => sum + Number(item.quantity), 0)} items`;
        details.append(id, meta);
        const total = document.createElement("strong");
        total.className = "order-card-total";
        total.textContent = window.CSMS.formatMoney(sale.total);
        const link = document.createElement("a");
        link.className = "button button-secondary";
        link.href = `order.html?order=${encodeURIComponent(sale.id)}`;
        link.textContent = "View details";
        card.append(details, total, link);
        results.append(card);
      });
      if (!matches.length) {
        const note = document.createElement("p");
        note.className = "lookup-no-results";
        note.textContent = "Your account has no matching orders yet.";
        results.append(note);
      }
    });
  }

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
    const search = document.getElementById("storeSearch");
    if (search) {
      search.addEventListener("input", () => renderProducts().catch(window.CSMS.notifyError));
      document.querySelectorAll("[data-category]").forEach((button) => {
        button.addEventListener("click", () => {
          selectedCategory = button.dataset.category;
          document.querySelectorAll("[data-category]").forEach((chip) => chip.classList.toggle("active", chip === button));
          renderProducts().catch(window.CSMS.notifyError);
        });
      });
      renderProducts().catch(window.CSMS.notifyError);
    }

    renderStoreCouponOffer();
    saveCart();
    setupCheckout();
    setupOrderConfirmation().catch(window.CSMS.notifyError);
    setupOrderLookup();
  }).catch(window.CSMS.notifyError);
})();
