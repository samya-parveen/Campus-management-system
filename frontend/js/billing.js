const catalog = [
  { id: 1, name: 'Notebook', category: 'Stationery', price: 48 },
  { id: 2, name: 'Pen Pack', category: 'Stationery', price: 120 },
  { id: 3, name: 'Water Bottle', category: 'Essentials', price: 220 },
  { id: 4, name: 'Campus Hoodie', category: 'Apparel', price: 850 },
  { id: 5, name: 'USB Cable', category: 'Electronics', price: 180 },
  { id: 6, name: 'Backpack', category: 'Accessories', price: 680 },
  { id: 7, name: 'Coffee Mug', category: 'Lifestyle', price: 260 },
  { id: 8, name: 'Sketchbook', category: 'Stationery', price: 150 },
  { id: 9, name: 'Laptop Sleeve', category: 'Accessories', price: 390 },
  { id: 10, name: 'Energy Drink', category: 'Food', price: 90 }
];

const cart = [
  { id: 1, name: 'Notebook', category: 'Stationery', price: 48, quantity: 2 },
  { id: 5, name: 'USB Cable', category: 'Electronics', price: 180, quantity: 1 }
];

const productList = document.getElementById('productList');
const cartTable = document.getElementById('cartTable');
const productSearch = document.getElementById('productSearch');
const cashReceived = document.getElementById('cashReceived');
const paymentMethod = document.getElementById('paymentMethod');

const subtotalValue = document.getElementById('subtotalValue');
const discountValue = document.getElementById('discountValue');
const taxValue = document.getElementById('taxValue');
const totalValue = document.getElementById('totalValue');
const changeValue = document.getElementById('changeValue');
const todaySales = document.getElementById('todaySales');

const money = new Intl.NumberFormat('en-IN', {
  style: 'currency',
  currency: 'INR',
  minimumFractionDigits: 2
});

function formatMoney(value) {
  return money.format(value || 0);
}

function getFilteredCatalog() {
  const searchText = productSearch.value.trim().toLowerCase();

  if (!searchText) {
    return catalog;
  }

  return catalog.filter((product) => {
    return (
      product.name.toLowerCase().includes(searchText) ||
      product.category.toLowerCase().includes(searchText) ||
      product.id.toString().includes(searchText)
    );
  });
}

function renderCatalog() {
  const visibleProducts = getFilteredCatalog();

  if (!visibleProducts.length) {
    productList.innerHTML = '<div class="empty-cart">No matching products found.</div>';
    return;
  }

  productList.innerHTML = visibleProducts
    .map((product) => {
      const inCart = cart.some((item) => item.id === product.id);

      return `
        <div class="product-item">
          <h4>${product.name}</h4>
          <div class="product-meta">
            <span>${product.category}</span>
            <strong>${formatMoney(product.price)}</strong>
          </div>
          <button class="add-product-btn" data-id="${product.id}" ${inCart ? 'disabled' : ''}>
            ${inCart ? 'Added' : 'Add to Bill'}
          </button>
        </div>
      `;
    })
    .join('');

  document.querySelectorAll('.add-product-btn').forEach((button) => {
    button.addEventListener('click', () => {
      const productId = Number(button.dataset.id);
      const selectedProduct = catalog.find((item) => item.id === productId);

      if (!selectedProduct) {
        return;
      }

      const existingItem = cart.find((item) => item.id === productId);
      if (!existingItem) {
        cart.push({ ...selectedProduct, quantity: 1 });
      }

      render();
    });
  });
}

function renderCart() {
  if (!cart.length) {
    cartTable.innerHTML = `
      <tr>
        <td colspan="5">
          <div class="empty-cart">Your cart is empty.</div>
        </td>
      </tr>
    `;
    return;
  }

  cartTable.innerHTML = cart
    .map(
      (item) => `
        <tr>
          <td>
            <strong>${item.name}</strong><br />
            <small>${item.category}</small>
          </td>
          <td>
            <div class="qty-box">
              <button class="qty-adjust" data-action="decrease" data-id="${item.id}">-</button>
              <span>${item.quantity}</span>
              <button class="qty-adjust" data-action="increase" data-id="${item.id}">+</button>
            </div>
          </td>
          <td>${formatMoney(item.price)}</td>
          <td>${formatMoney(item.price * item.quantity)}</td>
          <td>
            <button class="remove-btn" data-id="${item.id}">Remove</button>
          </td>
        </tr>
      `
    )
    .join('');

  document.querySelectorAll('.qty-adjust').forEach((button) => {
    button.addEventListener('click', () => {
      const productId = Number(button.dataset.id);
      const action = button.dataset.action;
      const item = cart.find((entry) => entry.id === productId);

      if (!item) {
        return;
      }

      if (action === 'increase') {
        item.quantity += 1;
      } else {
        item.quantity -= 1;
        if (item.quantity <= 0) {
          const index = cart.findIndex((entry) => entry.id === productId);
          cart.splice(index, 1);
        }
      }

      render();
    });
  });

  document.querySelectorAll('.remove-btn').forEach((button) => {
    button.addEventListener('click', () => {
      const productId = Number(button.dataset.id);
      const index = cart.findIndex((item) => item.id === productId);
      if (index >= 0) {
        cart.splice(index, 1);
      }
      render();
    });
  });
}

function calculateTotals() {
  const subtotal = cart.reduce((sum, item) => sum + item.price * item.quantity, 0);
  const discount = subtotal * 0.1;
  const tax = (subtotal - discount) * 0.05;
  const total = subtotal - discount + tax;
  const receivedAmount = Number(cashReceived.value || 0);
  const change = Math.max(receivedAmount - total, 0);

  subtotalValue.textContent = formatMoney(subtotal);
  discountValue.textContent = `- ${formatMoney(discount)}`;
  taxValue.textContent = formatMoney(tax);
  totalValue.textContent = formatMoney(total);
  changeValue.textContent = formatMoney(change);

  const salesToday = subtotal + Number(todaySales.dataset.value || 0);
  todaySales.textContent = formatMoney(salesToday);
  todaySales.dataset.value = salesToday;

  return { subtotal, discount, tax, total, change };
}

function render() {
  renderCatalog();
  renderCart();
  calculateTotals();
}

productSearch.addEventListener('input', renderCatalog);

cashReceived.addEventListener('input', () => {
  calculateTotals();
});

paymentMethod.addEventListener('change', () => {
  const method = paymentMethod.value;
  if (method === 'Cash') {
    cashReceived.focus();
  }
});

document.getElementById('processPaymentBtn').addEventListener('click', () => {
  const { total, change } = calculateTotals();

  if (!cart.length) {
    alert('Please add at least one item before processing payment.');
    return;
  }

  if (Number(cashReceived.value || 0) < total) {
    alert('Cash received is less than the total amount due.');
    return;
  }

  alert(
    `Payment processed successfully via ${paymentMethod.value}.\nTotal: ${formatMoney(total)}\nChange: ${formatMoney(change)}`
  );

  const newInvoice = `Invoice ${document.getElementById('invoiceNo').value} saved for ${document.getElementById('customerName').value}.`;
  console.log(newInvoice);
  cart.splice(0, cart.length);
  cashReceived.value = '';
  render();
});

document.getElementById('saveInvoiceBtn').addEventListener('click', () => {
  if (!cart.length) {
    alert('No items to save in the invoice.');
    return;
  }

  alert('Invoice saved successfully.');
});

document.getElementById('printInvoiceBtn').addEventListener('click', () => {
  window.print();
});

const invoiceDate = document.getElementById('invoiceDate');
invoiceDate.valueAsDate = new Date();

render();
