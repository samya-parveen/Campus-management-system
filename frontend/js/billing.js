(() => {
  "use strict";

  const productSelect = document.getElementById("productSelect");
  const cartTable = document.getElementById("cartTable");
  const cartForm = document.getElementById("addToCartForm");
  const couponInput = document.getElementById("couponCode");
  const couponMessage = document.getElementById("couponMessage");
  const cartKey = window.CSMS.cartKey;
  let cart = JSON.parse(sessionStorage.getItem(cartKey) || "[]");
  let appliedCoupon = "";

  function saveCart() {
    sessionStorage.setItem(cartKey, JSON.stringify(cart));
    const badge = document.getElementById("navCartCount");
    if (badge) badge.textContent = String(cart.reduce((sum, item) => sum + item.quantity, 0));
  }

  function setCouponMessage(message, isError = false) {
    couponMessage.textContent = message;
    couponMessage.classList.toggle("error", isError);
  }

  function featuredCouponMessage() {
    const coupon = window.CSMS.coupons.featured();
    if (!coupon) return "No coupon codes are available right now.";
    const saving = coupon.type === "fixed" ? window.CSMS.formatMoney(coupon.value) : `${coupon.value}%`;
    return `Try ${coupon.code} for ${saving} off.`;
  }

  async function render() {
    const currentTotals = cart.length
      ? await window.CSMS.calculateDiscount(cart, appliedCoupon)
      : { subtotal: 0, discount: 0, total: 0, couponError: "" };
    if (appliedCoupon && currentTotals.couponError) {
      appliedCoupon = "";
      setCouponMessage(currentTotals.couponError, true);
    }
    const availableProducts = window.CSMS.products.all();
    productSelect.replaceChildren(new Option("Choose a product to add…", ""));
    availableProducts.forEach((product) => {
      const option = new Option(`${product.name} · ${window.CSMS.formatMoney(product.price)} · ${product.quantity} in stock`, String(product.id));
      option.disabled = Number(product.quantity) <= 0;
      productSelect.append(option);
    });

    cartTable.replaceChildren();
    cart.forEach((item) => {
      const row = document.createElement("tr");
      const name = document.createElement("td");
      name.className = "cart-product";
      name.textContent = item.name;
      const price = document.createElement("td");
      price.textContent = window.CSMS.formatMoney(item.price);
      const quantity = document.createElement("td");
      const controls = document.createElement("span");
      controls.className = "qty-control";
      const decrease = document.createElement("button");
      decrease.type = "button";
      decrease.className = "qty-button";
      decrease.textContent = "−";
      decrease.setAttribute("aria-label", `Remove one ${item.name}`);
      decrease.addEventListener("click", () => updateQuantity(item.id, item.quantity - 1));
      const count = document.createElement("span");
      count.textContent = String(item.quantity);
      const increase = document.createElement("button");
      increase.type = "button";
      increase.className = "qty-button";
      increase.textContent = "+";
      increase.setAttribute("aria-label", `Add one ${item.name}`);
      increase.addEventListener("click", () => updateQuantity(item.id, item.quantity + 1));
      controls.append(decrease, count, increase);
      quantity.append(controls);
      const amount = document.createElement("td");
      amount.className = "align-right";
      amount.textContent = window.CSMS.formatMoney(item.price * item.quantity);
      const removeCell = document.createElement("td");
      const remove = document.createElement("button");
      remove.type = "button";
      remove.className = "remove-item";
      remove.textContent = "×";
      remove.setAttribute("aria-label", `Remove ${item.name} from basket`);
      remove.addEventListener("click", () => updateQuantity(item.id, 0));
      removeCell.append(remove);
      row.append(name, price, quantity, amount, removeCell);
      cartTable.append(row);
    });
    if (cart.length === 0) {
      const row = document.createElement("tr");
      const empty = document.createElement("td");
      empty.className = "billing-empty";
      empty.colSpan = 5;
      empty.textContent = "Your basket is waiting for its first item.";
      row.append(empty);
      cartTable.append(row);
      appliedCoupon = "";
      couponInput.value = "";
      setCouponMessage(featuredCouponMessage());
    }

    const count = cart.reduce((sum, item) => sum + item.quantity, 0);
    document.getElementById("basketItemCount").textContent = `${count} ${count === 1 ? "item" : "items"}`;
    document.getElementById("cartHint").textContent = count ? `${count} ${count === 1 ? "item" : "items"} ready for checkout.` : "Add products from your inventory.";
    const totals = cart.length
      ? await window.CSMS.calculateDiscount(cart, appliedCoupon)
      : { subtotal: 0, discount: 0, total: 0 };
    document.getElementById("subtotal").textContent = window.CSMS.formatMoney(totals.subtotal);
    document.getElementById("discountAmount").textContent = `−${window.CSMS.formatMoney(totals.discount)}`;
    document.getElementById("totalAmount").textContent = window.CSMS.formatMoney(totals.total);
    document.getElementById("completeSale").disabled = cart.length === 0;
    document.getElementById("completeSale").style.opacity = cart.length === 0 ? ".55" : "1";
    saveCart();
  }

  function refresh() {
    render().catch(window.CSMS.notifyError);
  }

  function updateQuantity(id, nextQuantity) {
    const item = cart.find((entry) => Number(entry.id) === Number(id));
    if (!item) return;
    if (nextQuantity <= 0) cart = cart.filter((entry) => Number(entry.id) !== Number(id));
    else {
      const product = window.CSMS.products.all().find((entry) => Number(entry.id) === Number(id));
      if (!product || nextQuantity > Number(product.quantity)) {
        window.CSMS.toast(`Only ${product ? product.quantity : 0} units are available.`, "error");
        return;
      }
      item.quantity = nextQuantity;
    }
    refresh();
  }

  function setup() {
  cartForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const productId = Number(productSelect.value);
    const quantity = Number(document.getElementById("itemQuantity").value);
    const product = window.CSMS.products.all().find((item) => Number(item.id) === productId);
    if (!product || !Number.isSafeInteger(quantity) || quantity <= 0) {
      window.CSMS.toast("Choose a product and a valid quantity.", "error");
      return;
    }
    const existing = cart.find((item) => Number(item.id) === productId);
    if (quantity + (existing?.quantity || 0) > Number(product.quantity)) {
      window.CSMS.toast(`Only ${product.quantity} units of ${product.name} are available.`, "error");
      return;
    }
    if (existing) existing.quantity += quantity;
    else cart.push({ id: product.id, name: product.name, category: product.category, price: Number(product.price), quantity });
    document.getElementById("itemQuantity").value = "1";
    productSelect.value = "";
    document.getElementById("printReceipt").hidden = true;
    refresh();
    window.CSMS.toast(`${product.name} added to the basket.`);
  });

  document.getElementById("clearCart").addEventListener("click", () => {
    cart = [];
    refresh();
  });

  document.getElementById("applyCoupon").addEventListener("click", async () => {
    const code = couponInput.value.trim().toUpperCase();
    if (cart.length === 0) {
      appliedCoupon = "";
      setCouponMessage("Add items before applying a discount.", true);
    } else if (!code) {
      appliedCoupon = "";
      setCouponMessage("Enter a discount code to apply.", true);
    } else {
      try {
        const result = await window.CSMS.calculateDiscount(cart, code);
        if (result.couponError) {
        appliedCoupon = "";
        setCouponMessage(result.couponError, true);
        } else {
          appliedCoupon = code;
          setCouponMessage(`${code} applied. You saved ${window.CSMS.formatMoney(result.discount)}.`);
        }
      } catch (error) {
        appliedCoupon = "";
        setCouponMessage(error.message, true);
      }
    }
    refresh();
  });

  couponInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      document.getElementById("applyCoupon").click();
    }
  });

  document.getElementById("completeSale").addEventListener("click", async (event) => {
    if (cart.length === 0) {
      window.CSMS.toast("Add at least one product before completing a sale.", "error");
      return;
    }
    const latestProducts = window.CSMS.products.all();
    const unavailableItem = cart.find((item) => {
      const product = latestProducts.find((entry) => Number(entry.id) === Number(item.id));
      return !product || Number(product.quantity) < item.quantity;
    });
    if (unavailableItem) {
      window.CSMS.toast(`${unavailableItem.name} no longer has enough stock. Update your basket.`, "error");
      refresh();
      return;
    }
    const button = event.currentTarget;
    button.disabled = true;
    try {
      const completed = await window.CSMS.sales.add({
        channel: "admin",
        payment: document.getElementById("paymentMethod").value,
        couponCode: appliedCoupon,
        items: cart.map((item) => ({ id: item.id, quantity: item.quantity }))
      });
      cart = [];
      appliedCoupon = "";
      await render();
      const receipt = document.getElementById("printReceipt");
      document.getElementById("receiptDetails").textContent = `${completed.id} · ${completed.items.length} ${completed.items.length === 1 ? "item" : "items"} · ${completed.payment} · ${window.CSMS.formatMoney(completed.total)}`;
      receipt.hidden = false;
      window.CSMS.toast("Sale completed. Inventory and reports are up to date.");
    } catch (error) {
      window.CSMS.notifyError(error);
    } finally {
      button.disabled = false;
    }
  });

  document.getElementById("printReceiptButton").addEventListener("click", () => window.print());
  refresh();
  }

  window.CSMS.ready.then((ready) => {
    if (ready) setup();
  }).catch(window.CSMS.notifyError);
})();