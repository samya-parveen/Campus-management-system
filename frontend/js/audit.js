(() => {
  "use strict";

  const search = document.getElementById("auditSearch");
  const actionFilter = document.getElementById("auditAction");
  const periodFilter = document.getElementById("auditPeriod");
  const rows = document.getElementById("auditRows");
  const empty = document.getElementById("auditEmpty");
  let entries = [];

  function getVisibleEntries() {
    const query = search.value.trim().toLowerCase();
    const selectedAction = actionFilter.value;
    const days = Number(periodFilter.value);
    const threshold = days ? Date.now() - days * 24 * 60 * 60 * 1000 : 0;
    return entries
      .filter((entry) => {
        const searchable = `${entry.action} ${entry.user} ${entry.email} ${entry.details}`.toLowerCase();
        return (!query || searchable.includes(query)) &&
          (!selectedAction || entry.action === selectedAction) &&
          (!threshold || new Date(entry.date).getTime() >= threshold);
      })
      .sort((first, second) => new Date(second.date) - new Date(first.date));
  }

  function makeCell(text, className = "") {
    const cell = document.createElement("td");
    if (className) cell.className = className;
    cell.textContent = text;
    return cell;
  }

  function render() {
    const allEntries = entries;
    const visibleEntries = getVisibleEntries();
    const today = new Date().toDateString();
    document.getElementById("auditTotal").textContent = String(allEntries.length);
    document.getElementById("auditToday").textContent = String(allEntries.filter((entry) => new Date(entry.date).toDateString() === today).length);
    document.getElementById("auditVisible").textContent = String(visibleEntries.length);
    document.getElementById("auditCount").textContent = String(visibleEntries.length);
    rows.replaceChildren();
    visibleEntries.forEach((entry) => {
      const row = document.createElement("tr");
      const activity = document.createElement("td");
      const title = document.createElement("strong");
      title.className = "audit-action";
      title.textContent = entry.action;
      const details = document.createElement("small");
      details.className = "audit-details";
      details.textContent = entry.details;
      activity.append(title, details);
      const user = document.createElement("td");
      const name = document.createElement("strong");
      name.className = "audit-user";
      name.textContent = entry.user;
      const account = document.createElement("small");
      account.className = "audit-details";
      account.textContent = entry.email || entry.role;
      user.append(name, account);
      const date = new Date(entry.date);
      const dateText = Number.isNaN(date.getTime())
        ? "Unknown date"
        : new Intl.DateTimeFormat("en-IN", { day: "numeric", month: "short", year: "numeric", hour: "numeric", minute: "2-digit" }).format(date);
      row.append(activity, user, makeCell(dateText));
      rows.append(row);
    });
    empty.hidden = visibleEntries.length > 0;
  }

  function csvValue(value) {
    const text = value === null || value === undefined ? "" : String(value);
    const safeText = /^[\t\r=+\-@]/.test(text) ? `'${text}` : text;
    return `"${safeText.replace(/"/g, '""')}"`;
  }

  function exportAudit() {
    const entries = getVisibleEntries();
    const csv = [
      ["Date", "User", "Email", "Role", "Action", "Details"],
      ...entries.map((entry) => [entry.date, entry.user, entry.email, entry.role, entry.action, entry.details])
    ].map((line) => line.map(csvValue).join(",")).join("\r\n");
    const url = URL.createObjectURL(new Blob([csv], { type: "text/csv;charset=utf-8" }));
    const link = document.createElement("a");
    link.href = url;
    link.download = "campus-store-audit.csv";
    link.click();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  window.CSMS.ready.then((ready) => {
    if (!ready) return;
    entries = window.CSMS.audit.all();
    [...new Set(entries.map((entry) => entry.action))].sort().forEach((action) => {
      const option = document.createElement("option");
      option.value = action;
      option.textContent = action;
      actionFilter.append(option);
    });
    document.getElementById("exportAudit").addEventListener("click", exportAudit);
    search.addEventListener("input", render);
    [actionFilter, periodFilter].forEach((control) => control.addEventListener("change", render));
    render();
  }).catch(window.CSMS.notifyError);
})();
