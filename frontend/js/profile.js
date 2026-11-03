(() => {
  "use strict";

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
  const user = window.CSMS.auth.current();
  if (!user) return;

  const admin = user.role === "admin";
  document.getElementById(admin ? "adminProfile" : "customerProfile").hidden = false;
  const fieldIds = admin
    ? { name: "adminName", email: "adminEmail", phone: "adminPhone", error: "adminProfileError", form: "adminProfileForm" }
    : { name: "customerName", email: "customerEmail", phone: "customerPhone", error: "customerProfileError", form: "customerProfileForm" };
  const nameField = document.getElementById(fieldIds.name);
  const emailField = document.getElementById(fieldIds.email);
  const phoneField = document.getElementById(fieldIds.phone);
  nameField.value = user.name || "";
  emailField.value = user.email || "";
  phoneField.value = user.phone || "";

  if (admin) {
    const initials = (user.name || "Store Admin").trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
    document.getElementById("adminAvatar").textContent = initials;
    document.getElementById("adminTopAvatar").textContent = initials;
    document.getElementById("adminSidebarName").textContent = user.name || "Store Admin";
    document.getElementById("adminToday").textContent = new Intl.DateTimeFormat("en-IN", { weekday: "short", day: "numeric", month: "short", year: "numeric" }).format(new Date());
  } else {
    const firstName = (user.name || "there").trim().split(/\s+/)[0];
    document.getElementById("customerGreeting").textContent = firstName;
    document.getElementById("customerAvatar").textContent = (user.name || "C").trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
  }

  document.getElementById(fieldIds.form).addEventListener("submit", async (event) => {
    event.preventDefault();
    const error = document.getElementById(fieldIds.error);
    const name = nameField.value.trim();
    if (name.length < 2) {
      error.textContent = "Enter your full name (at least 2 characters).";
      error.hidden = false;
      return;
    }
    try {
      const updatedUser = await window.CSMS.auth.updateProfile({ name, phone: phoneField.value.trim() });
      error.hidden = true;
      const initials = updatedUser.name.trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
      document.getElementById(admin ? "adminSidebarName" : "customerGreeting").textContent = admin ? updatedUser.name : updatedUser.name.trim().split(/\s+/)[0];
      document.getElementById(admin ? "adminAvatar" : "customerAvatar").textContent = initials;
      if (admin) document.getElementById("adminTopAvatar").textContent = initials;
      window.CSMS.toast("Your profile has been updated.");
    } catch (errorValue) {
      error.textContent = errorValue.message;
      error.hidden = false;
    }
  });

  const passwordForm = document.getElementById(admin ? "adminPasswordForm" : "customerPasswordForm");
  passwordForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    const prefix = admin ? "admin" : "customer";
    const error = document.getElementById(`${prefix}PasswordError`);
    const currentPassword = document.getElementById(`${prefix}CurrentPassword`).value;
    const newPassword = document.getElementById(`${prefix}NewPassword`).value;
    const confirmPassword = document.getElementById(`${prefix}ConfirmPassword`).value;
    if (newPassword.length < 8) {
      error.textContent = "Choose a new password with at least 8 characters.";
      error.hidden = false;
      return;
    }
    if (newPassword !== confirmPassword) {
      error.textContent = "The new passwords do not match.";
      error.hidden = false;
      return;
    }
    try {
      await window.CSMS.auth.changePassword(currentPassword, newPassword);
    } catch (errorValue) {
      error.textContent = errorValue.message;
      error.hidden = false;
      return;
    }
    passwordForm.reset();
    error.hidden = true;
    window.CSMS.toast("Your password has been updated.");
  });
  }).catch(window.CSMS.notifyError);
})();
