# Campus Store Management System

A campus-store website backed by SQLite and a C++ REST API. The same frontend and API contract can run with either the procedural C++ backend or the OOP C++ backend for the case study.

## What is included

- Sign-in, customer account creation, database-backed sessions, and profile/password updates.
- Admin product, discount, coupon, reports, and audit pages.
- Customer product browsing, basket, coupon quotes, checkout, and order history.
- SQLite persistence for accounts, inventory, promotions, sales, sessions, and audit records.
- Procedural and OOP C++ implementations of the case-study pricing rules, using identical API routes and database schema.

## Run the website on Windows

You need PowerShell, `g++` and `gcc` (MinGW), and an internet connection the first time you run it. The launcher downloads checksum-verified SQLite and nlohmann/json dependencies to `%LOCALAPPDATA%\CampusStore\deps`; generated executables are kept in a temporary directory.

From the repository root, start the OOP backend:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-website.ps1 -Backend oop
```

Keep the terminal open and visit [http://127.0.0.1:8080](http://127.0.0.1:8080). Stop the server with Ctrl+C. To use the procedural version, stop the first server and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-website.ps1 -Backend procedural
```

Optional arguments:

```powershell
# Choose a port
.\scripts\run-website.ps1 -Backend oop -Port 8090

# Use an isolated database for a fair backend comparison
.\scripts\run-website.ps1 -Backend procedural -DatabasePath .\backend\data\procedural-test.db
```

The default database is `backend/data/campus_store.db` and persists between runs. A new database is initialized with demo data:

- Admin: `demo@campus.edu` / `campus123`
- Customer: `customer@campus.edu` / `customer123`

New sign-ups create customer accounts only; admin access is restricted to the provisioned admin account. Do not use the demo passwords or real personal/payment data outside a local classroom demonstration.

## Case study

Both backend modes implement the same HTTP routes and database schema. They differ in the C++ pricing/business-rule implementation, while the single frontend uses the same REST API for either mode. The server exposes `/api/pricing/case-study` for comparing category price, percentage discount, and coupon results.

Run the standalone shared acceptance tests and sample programs:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-case-study.ps1
```

Use `-TestsOnly` to omit the sample programs. See [docs/case-study-guide.md](docs/case-study-guide.md) for the requirements, expected results, and measurement procedure. Results in `results/measurements.csv` must be measured from a controlled run; do not infer development time from the completed code.

## Notes and limitations

The website server binds only to `127.0.0.1` and uses a small synchronous HTTP server intended for local coursework demonstrations. It is not a production deployment server. Before deployment, replace it with a maintained HTTP framework and add production-grade transport, operational, and account-management controls. The demo payment options do not charge money.
