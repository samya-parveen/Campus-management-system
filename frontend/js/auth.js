(() => {
  "use strict";

  const isLoginPage = window.location.pathname.endsWith("/login.html");
  const isStorePage = window.location.pathname.includes("/store/");
  const isProfilePage = window.location.pathname.endsWith("/profile.html");
  const isEntryPage = window.location.pathname.endsWith("/index.html") && !isStorePage;
  const appRoot = new URL(isStorePage ? "../" : "./", window.location.href);

  function redirectForRole(role) {
    const destination = role === "customer" ? "store/index.html" : "dashboard.html";
    window.location.replace(new URL(destination, appRoot).href);
  }

  function showAuthMode(mode) {
    const creatingAccount = mode === "register";
    const loginForm = document.getElementById("loginForm");
    const registerForm = document.getElementById("registerForm");
    if (!loginForm || !registerForm) return;
    loginForm.hidden = creatingAccount;
    registerForm.hidden = !creatingAccount;
    document.getElementById("signInTab").classList.toggle("active", !creatingAccount);
    document.getElementById("createAccountTab").classList.toggle("active", creatingAccount);
    document.getElementById("signInTab").setAttribute("aria-selected", String(!creatingAccount));
    document.getElementById("createAccountTab").setAttribute("aria-selected", String(creatingAccount));
    document.getElementById("authEyebrow").textContent = creatingAccount ? "YOUR CAMPUS STARTS HERE" : "WELCOME BACK";
    document.getElementById("authHeading").textContent = creatingAccount ? "Create your account" : "Sign in to your store";
    document.getElementById("authDescription").textContent = creatingAccount
      ? "Sign up to shop and keep your campus orders together."
      : "Your account takes you to the right campus workspace.";
    document.getElementById("demoNote").hidden = creatingAccount;
    document.getElementById("loginError").hidden = true;
    document.getElementById("registerError").hidden = true;
  }

  function setupAuthForms() {
    const loginForm = document.getElementById("loginForm");
    if (!loginForm) return;
    const registerForm = document.getElementById("registerForm");
    const password = document.getElementById("password");
    const loginError = document.getElementById("loginError");
    const registerError = document.getElementById("registerError");

    document.getElementById("signInTab").addEventListener("click", () => showAuthMode("login"));
    document.getElementById("createAccountTab").addEventListener("click", () => showAuthMode("register"));

    loginForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const email = document.getElementById("email").value.trim().toLowerCase();
      const button = loginForm.querySelector('button[type="submit"]');
      button.disabled = true;
      try {
        const account = await window.CSMS.auth.authenticate(email, password.value);
        redirectForRole(account.role);
      } catch (error) {
        loginError.textContent = error.message;
        loginError.hidden = false;
      } finally {
        button.disabled = false;
      }
    });

    registerForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const name = document.getElementById("registerName").value.trim();
      const email = document.getElementById("registerEmail").value.trim().toLowerCase();
      const phone = document.getElementById("registerPhone").value.trim();
      const newPassword = document.getElementById("registerPassword").value;
      const confirmPassword = document.getElementById("confirmPassword").value;
      if (name.length < 2) {
        registerError.textContent = "Enter your full name (at least 2 characters).";
        registerError.hidden = false;
        return;
      }
      if (newPassword.length < 8) {
        registerError.textContent = "Choose a password with at least 8 characters.";
        registerError.hidden = false;
        return;
      }
      if (newPassword !== confirmPassword) {
        registerError.textContent = "The passwords do not match.";
        registerError.hidden = false;
        return;
      }
      const button = registerForm.querySelector('button[type="submit"]');
      button.disabled = true;
      try {
        const result = await window.CSMS.auth.register({ name, email, phone }, newPassword);
        redirectForRole(result.user.role);
      } catch (error) {
        registerError.textContent = error.message;
        registerError.hidden = false;
      } finally {
        button.disabled = false;
      }
    });

    document.getElementById("togglePassword").addEventListener("click", (event) => {
      const toggle = event.currentTarget;
      const reveal = password.type === "password";
      password.type = reveal ? "text" : "password";
      toggle.textContent = reveal ? "Hide" : "Show";
      toggle.setAttribute("aria-label", reveal ? "Hide password" : "Show password");
    });
  }

  function updateUserChrome(user) {
    const name = user.name || "Store Admin";
    const initials = name.trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
    const nameElement = document.getElementById("sidebarName");
    const avatar = document.getElementById("sidebarAvatar");
    const topAvatar = document.getElementById("topAvatar");
    const greeting = document.getElementById("welcomeName");
    const todayLabel = document.getElementById("todayLabel");
    if (nameElement) nameElement.textContent = name;
    if (avatar) avatar.textContent = initials;
    if (topAvatar) topAvatar.textContent = initials;
    if (greeting) greeting.textContent = name;
    if (todayLabel) todayLabel.textContent = new Intl.DateTimeFormat("en-IN", {
      weekday: "short", day: "numeric", month: "short", year: "numeric"
    }).format(new Date());
  }

  async function initialize() {
    setupAuthForms();
    let user = null;
    if (window.CSMS.auth.hasToken()) {
      try {
        user = await window.CSMS.auth.me();
      } catch {
        window.CSMS.auth.clear();
      }
    }

    if (!user) {
      if (!isLoginPage && !isEntryPage) {
        window.location.replace(new URL("login.html", appRoot).href);
      }
      return false;
    }

    const role = user.role;
    if (isLoginPage || isEntryPage) {
      redirectForRole(role);
      return false;
    }
    if (isStorePage && role !== "customer") {
      redirectForRole("admin");
      return false;
    }
    if (!isLoginPage && !isEntryPage && !isStorePage && !isProfilePage && role !== "admin") {
      redirectForRole("customer");
      return false;
    }

    updateUserChrome(user);
    await window.CSMS.loadData(user);
    document.querySelectorAll("[data-logout]").forEach((button) => {
      button.addEventListener("click", async () => {
        try {
          await window.CSMS.auth.signOut();
        } catch (error) {
          window.CSMS.notifyError(error);
        }
        if (role === "customer") sessionStorage.removeItem(window.CSMS.customerCartKey);
        window.location.replace(new URL("login.html", appRoot).href);
      });
    });

    const cartBadge = document.getElementById("navCartCount");
    if (cartBadge) {
      const cart = JSON.parse(sessionStorage.getItem(window.CSMS.cartKey) || "[]");
      cartBadge.textContent = String(cart.reduce((sum, item) => sum + Number(item.quantity || 0), 0));
    }
    return true;
  }

  window.CSMS.ready = initialize().catch((error) => {
    window.CSMS.notifyError(error);
    return false;
  });
})();
