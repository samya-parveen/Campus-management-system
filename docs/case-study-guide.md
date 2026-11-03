# Procedural C++ vs OOP C++ case study

## What is being compared

This study compares two C++ programming styles while keeping the language, compiler, requirements, test cases, and sample input the same:

- `procedural-version/` uses `struct` data and free functions that receive a store.
- `backend/include/Store.h` and `backend/include/src/` use classes, encapsulation, inheritance, and polymorphism.
- `tests/acceptance_suite.hpp` runs the same ten functional checks against either implementation through a small adapter.
- `tests/billing-test.cpp` also checks OOP `Product` copy and live-object counting.

The folder map follows the repository's existing structure: `backend/` is the OOP version and `procedural-version/` is the procedural version. This is not a comparison of C against C++, which would mix programming style with language differences. The standalone acceptance programs compare both implementations. The website can also be started in either backend mode; both modes expose the same REST API and use the same SQLite schema, while the category-pricing and discount calculations are supplied by the selected C++ implementation. The current source implements both requested changes (CR1 Stationery pricing and CR2 coupon discounts) in both styles.

## Run the connected website in both modes

The website launcher builds the selected backend and serves the frontend and API together:

```powershell
.\scripts\run-website.ps1 -Backend procedural
.\scripts\run-website.ps1 -Backend oop
```

Stop the first server with Ctrl+C before starting the second. Sign in at `http://127.0.0.1:8080` and exercise the same workflow in each mode. For an isolated comparison, give each run a different fresh database path with `-DatabasePath`; both databases receive the same seed data. Compare the output of `/api/pricing/case-study` as well as the website behavior. The default database is persistent, so repeating a workflow against it changes stock, orders, coupon usage, and audit records. See the repository [README](../README.md) for launcher prerequisites and demo accounts.

The built-in HTTP server binds only to localhost and is intended for a classroom demo, not production deployment.

## Requirements represented in the implementation

Both versions support registering and deleting products, duplicate-ID and input validation, Book and Accessory category pricing, percentage discounts, billing, and audit output. Stationery receives its 5% category reduction before any percentage discount. A coupon is subtracted after the percentage discount, and the result cannot be negative.

The OOP version additionally demonstrates constructors, private data, inheritance and virtual dispatch for product types, overloaded `discount` functions, an inline getter, a static live-object counter, a friend audit function, and `Bill` operator overloading. Its copy constructor/destructor keep the live-object counter correct.

## Build and run

From the repository root, with GCC/MinGW and PowerShell available:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-case-study.ps1
```

The runner compiles both acceptance-test programs and both sample programs, executes the tests, then prints each sample. To omit the demos:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-case-study.ps1 -TestsOnly
```

The execution-policy option is scoped to the PowerShell process running this command; it does not persistently change the computer's policy.
The runner compiles into a uniquely named temporary directory outside the repository and removes it when finished. You do not need to open or manage generated executable files.

## Verified output

Both demos use identical sample products and produce the same pricing:

| Item / operation | Expected result |
|---|---:|
| Book, base ₹500, category price | ₹450.00 |
| Same Book after 10% discount | ₹405.00 |
| Accessory, base ₹800, category price | ₹840.00 |
| Same Accessory after 10% discount and ₹20 coupon | ₹736.00 |
| Combined bill | ₹1,141.00 |
| Stationery, base ₹200, category price | ₹190.00 |
| Stationery after 10% discount | ₹171.00 |

The shared functional suite passes **10/10** in each implementation. The OOP-specific copy-safety check passes **1/1**. The full test cases and expected outputs are listed in `tests/acceptance-cases.csv`.

## How to complete the measurements sheet

Record results in `results/measurements.csv` only after conducting the same experiment for both versions. Use the same programmer(s), computer, compiler/version, requirements, starting feature set, change-request wording, and test procedure. Keep the CR1 and CR2 runs separate:

1. Start both implementations from the same pre-change assignment baseline (the version before CR1 and CR2).
2. For initial-development time, time the work needed to meet the original requirements and stop when the same original tests pass.
3. For CR1, start from that baseline and time adding Stationery with the required 5% reduction, including retesting.
4. For CR2, start from the same baseline plus CR1 and time adding the coupon-after-percentage behavior, including retesting.
5. Record elapsed minutes from the same start/stop rule. Count nonblank, noncomment source lines consistently. Count each existing function once if it was changed for that request; count new/modified source files separately.
6. Record test results as passed/total and include failed-test repair and retest time in the designated row.

This repository contains the completed post-change implementations, not the pre-change assignment snapshot. The original starting code in this workspace was empty placeholders, so initial-development and CR1/CR2 elapsed times cannot be reconstructed fairly from the finished code. Keep those cells blank until the experiment is timed from a valid baseline; do not treat compiler runs or generated output as measured development time.

Calculate change-time reduction only when both comparable change times have been measured:

`((procedural time - OOP time) / procedural time) * 100`

A positive result means the OOP change took less time; a negative result means it took more. Report the measured values and experimental conditions rather than claiming either style is universally faster. The functional test results establish behavioral parity, not a time-performance result.
