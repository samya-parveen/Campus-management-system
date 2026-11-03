(() => {
  "use strict";

  const discountForm = document.getElementById("discountForm");
  const couponForm = document.getElementById("couponForm");
  const categoryName = (category) => ({ All: "All products", Book: "Books", Accessory: "Accessories", Stationery: "Stationery" })[category] || category;

  function button(label, className, action) {
    const element = document.createElement("button");
    element.type = "button";
    element.className = className;
    element.textContent = label;
    element.addEventListener("click", async () => {
      try {
        await action();
      } catch (error) {
        window.CSMS.notifyError(error);
      }
    });
    return element;
  }

  function setFormError(element, message) {
    element.textContent = message;
    element.hidden = !message;
  }

  function renderDiscounts() {
    const list = document.getElementById("discountList");
    if (!list) return;
    const discounts = window.CSMS.discounts.all();
    document.getElementById("discountCount").textContent = String(discounts.length);
    list.replaceChildren();
    if (!discounts.length) {
      const empty = document.createElement("div");
      empty.className = "manager-empty";
      empty.textContent = "No product discounts yet. Create one to start an automatic offer.";
      list.append(empty);
      return;
    }
    discounts.forEach((discount) => {
      const card = document.createElement("article");
      card.className = "manager-card";
      const icon = document.createElement("span");
      icon.className = "manager-card-icon";
      icon.textContent = "％";
      const details = document.createElement("div");
      details.className = "manager-card-details";
      const title = document.createElement("strong");
      title.textContent = discount.name;
      const scope = document.createElement("small");
      scope.textContent = `${categoryName(discount.category)} · ${discount.value}% off${discount.endsAt ? ` · Ends ${window.CSMS.formatDate(`${discount.endsAt}T12:00:00`, { day: "numeric", month: "short" })}` : ""}`;
      details.append(title, scope);
      const value = document.createElement("span");
      value.className = `manager-value${discount.active ? "" : " inactive"}`;
      value.textContent = discount.active ? `${discount.value}%` : "Paused";
      const actions = document.createElement("div");
      actions.className = "manager-actions";
      actions.append(
        button("Edit", "row-action", () => editDiscount(discount)),
        button(discount.active ? "Pause" : "Activate", "row-action", async () => {
          await window.CSMS.discounts.save({ ...discount, active: !discount.active });
          renderDiscounts();
        }),
        button("Delete", "row-action delete", async () => {
          if (!window.confirm(`Delete the "${discount.name}" discount?`)) return;
          await window.CSMS.discounts.remove(discount.id);
          renderDiscounts();
          window.CSMS.toast("Discount removed.");
        })
      );
      card.append(icon, details, value, actions);
      list.append(card);
    });
  }

  function editDiscount(discount) {
    document.getElementById("discountId").value = discount.id;
    document.getElementById("discountName").value = discount.name;
    document.getElementById("discountCategory").value = discount.category;
    document.getElementById("discountValue").value = String(discount.value);
    document.getElementById("discountStarts").value = discount.startsAt || "";
    document.getElementById("discountEnds").value = discount.endsAt || "";
    document.getElementById("cancelDiscountEdit").hidden = false;
    document.getElementById("discountName").focus();
  }

  function setupDiscountForm() {
    if (!discountForm) return;
    const error = document.getElementById("discountError");
    const reset = () => {
      discountForm.reset();
      document.getElementById("discountId").value = "";
      document.getElementById("cancelDiscountEdit").hidden = true;
      setFormError(error, "");
    };
    document.getElementById("cancelDiscountEdit").addEventListener("click", reset);
    discountForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const id = document.getElementById("discountId").value || `discount-${Date.now()}`;
      const name = document.getElementById("discountName").value.trim();
      const category = document.getElementById("discountCategory").value;
      const value = Number(document.getElementById("discountValue").value);
      const startsAt = document.getElementById("discountStarts").value;
      const endsAt = document.getElementById("discountEnds").value;
      if (!name || !["All", "Book", "Accessory", "Stationery"].includes(category) || !Number.isInteger(value) || value < 1 || value > 90) {
        setFormError(error, "Enter a name, valid category, and discount between 1% and 90%.");
        return;
      }
      if (startsAt && endsAt && startsAt > endsAt) {
        setFormError(error, "The end date must be on or after the start date.");
        return;
      }
      const existing = window.CSMS.discounts.all().find((item) => item.id === id);
      try {
        await window.CSMS.discounts.save({ id, name, category, value, startsAt, endsAt, active: existing?.active ?? true });
        reset();
        renderDiscounts();
        window.CSMS.toast("Product discount saved.");
      } catch (saveError) {
        setFormError(error, saveError.message);
      }
    });
  }

  function couponStatus(coupon) {
    if (!coupon.active) return "Paused";
    if (coupon.expiresAt && new Date(`${coupon.expiresAt}T23:59:59`) < new Date()) return "Expired";
    if (Number(coupon.maxUses) > 0 && Number(coupon.usageCount || 0) >= Number(coupon.maxUses)) return "Limit reached";
    return "Active";
  }

  function renderCoupons() {
    const list = document.getElementById("couponList");
    if (!list) return;
    const coupons = window.CSMS.coupons.all();
    document.getElementById("couponCount").textContent = String(coupons.length);
    list.replaceChildren();
    if (!coupons.length) {
      const empty = document.createElement("div");
      empty.className = "manager-empty";
      empty.textContent = "No coupon codes. Create one to share a special offer with customers.";
      list.append(empty);
      return;
    }
    coupons.forEach((coupon) => {
      const status = couponStatus(coupon);
      const card = document.createElement("article");
      card.className = "coupon-manager-card";
      const head = document.createElement("div");
      head.className = "coupon-card-head";
      const code = document.createElement("strong");
      code.className = "coupon-code-display";
      code.textContent = coupon.code;
      const badge = document.createElement("span");
      badge.className = `coupon-state${status === "Active" ? " active" : ""}`;
      badge.textContent = status;
      head.append(code, badge);
      const saving = document.createElement("p");
      saving.className = "coupon-saving";
      saving.textContent = coupon.type === "fixed" ? `${window.CSMS.formatMoney(coupon.value)} off` : `${coupon.value}% off`;
      const terms = document.createElement("small");
      terms.className = "coupon-terms";
      const scope = categoryName(coupon.category).toLowerCase();
      const minimum = Number(coupon.minimumPurchase) > 0 ? ` · min. ${window.CSMS.formatMoney(coupon.minimumPurchase)}` : "";
      const limit = Number(coupon.maxUses) > 0 ? ` · ${coupon.usageCount || 0}/${coupon.maxUses} used` : ` · ${coupon.usageCount || 0} redeemed`;
      terms.textContent = `${scope}${minimum}${coupon.expiresAt ? ` · until ${window.CSMS.formatDate(`${coupon.expiresAt}T12:00:00`, { day: "numeric", month: "short", year: "numeric" })}` : ""}${limit}`;
      const actions = document.createElement("div");
      actions.className = "manager-actions";
      actions.append(
        button("Edit", "row-action", () => editCoupon(coupon)),
        button(coupon.active ? "Pause" : "Activate", "row-action", async () => {
          await window.CSMS.coupons.save({ ...coupon, active: !coupon.active });
          renderCoupons();
        }),
        button("Delete", "row-action delete", async () => {
          if (!window.confirm(`Delete coupon "${coupon.code}"?`)) return;
          await window.CSMS.coupons.remove(coupon.code);
          renderCoupons();
          window.CSMS.toast("Coupon removed.");
        })
      );
      card.append(head, saving, terms, actions);
      list.append(card);
    });
  }

  function editCoupon(coupon) {
    document.getElementById("editingCouponCode").value = coupon.code;
    document.getElementById("couponCode").value = coupon.code;
    document.getElementById("couponType").value = coupon.type;
    document.getElementById("couponValue").value = String(coupon.value);
    document.getElementById("couponCategory").value = coupon.category;
    document.getElementById("couponMinimum").value = String(coupon.minimumPurchase || 0);
    document.getElementById("couponLimit").value = String(coupon.maxUses || 0);
    document.getElementById("couponExpiry").value = coupon.expiresAt || "";
    document.getElementById("cancelCouponEdit").hidden = false;
    document.getElementById("couponCode").focus();
  }

  function setupCouponForm() {
    if (!couponForm) return;
    const error = document.getElementById("couponError");
    const codeField = document.getElementById("couponCode");
    const typeField = document.getElementById("couponType");
    const valueField = document.getElementById("couponValue");
    const reset = () => {
      couponForm.reset();
      document.getElementById("editingCouponCode").value = "";
      document.getElementById("couponMinimum").value = "0";
      document.getElementById("couponLimit").value = "0";
      document.getElementById("cancelCouponEdit").hidden = true;
      valueField.max = "";
      setFormError(error, "");
    };
    typeField.addEventListener("change", () => {
      valueField.max = typeField.value === "percentage" ? "90" : "";
      valueField.placeholder = typeField.value === "percentage" ? "10" : "100";
    });
    document.getElementById("cancelCouponEdit").addEventListener("click", reset);
    couponForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const editingCode = document.getElementById("editingCouponCode").value;
      const code = codeField.value.trim().toUpperCase();
      const type = typeField.value;
      const value = Number(valueField.value);
      const category = document.getElementById("couponCategory").value;
      const minimumPurchase = Number(document.getElementById("couponMinimum").value);
      const maxUses = Number(document.getElementById("couponLimit").value);
      const expiresAt = document.getElementById("couponExpiry").value;
      const conflict = window.CSMS.coupons.all().some((coupon) => coupon.code === code && coupon.code !== editingCode);
      if (!/^[A-Z0-9_-]{3,20}$/.test(code) || conflict) {
        setFormError(error, conflict ? "That coupon code already exists." : "Use 3–20 letters, numbers, dashes, or underscores.");
        return;
      }
      if ((type === "percentage" && (!Number.isInteger(value) || value < 1 || value > 90)) || (type === "fixed" && (!Number.isFinite(value) || value < 1)) || !Number.isFinite(minimumPurchase) || minimumPurchase < 0 || !Number.isSafeInteger(maxUses) || maxUses < 0) {
        setFormError(error, "Enter a valid discount, minimum purchase, and whole-number redemption limit.");
        return;
      }
      if (expiresAt && expiresAt < new Date().toISOString().slice(0, 10)) {
        setFormError(error, "The expiry date must be today or later.");
        return;
      }
      try {
        if (editingCode) {
          const old = window.CSMS.coupons.all().find((coupon) => coupon.code === editingCode);
          if (code !== editingCode) await window.CSMS.coupons.remove(editingCode);
          await window.CSMS.coupons.save({ code, type, value, category, minimumPurchase, maxUses, expiresAt, active: old?.active ?? true, usageCount: old?.usageCount || 0 });
        } else {
          await window.CSMS.coupons.save({ code, type, value, category, minimumPurchase, maxUses, expiresAt, active: true });
        }
        reset();
        renderCoupons();
        window.CSMS.toast("Coupon code saved and ready to share.");
      } catch (saveError) {
        setFormError(error, saveError.message);
      }
    });
  }

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
    setupDiscountForm();
    setupCouponForm();
    renderDiscounts();
    renderCoupons();
  }).catch(window.CSMS.notifyError);
})();
