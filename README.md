# Student Management System (C++17 / SQLite)

A modular console-based Student Management System with Admin and Student roles.

## Features

- Admin and Student login
- Add and display student records
- Attendance management and attendance percentage
- Report students with marks below 35%
- Student fee setup and fee payment in multiple installments
- Installment payment history with timestamps
- Student self-service profile, attendance and fee views
- Password policy: 8-10 characters, at least one digit and one symbol
- Password storage using PBKDF2-HMAC-SHA256 with a fresh random salt per account

## Files

- `main.cpp`: application entry point and login flow
- `database.h/.cpp`: SQLite connection, schema and default accounts
- `auth.h/.cpp`: password policy, PBKDF2 hashing, verification and login
- `student.h/.cpp`: student account and profile functions
- `attendance.h/.cpp`: attendance operations
- `fees.h/.cpp`: total fees, installments and payment history
- `reports.h/.cpp`: below-35%-marks report
- `admin.h/.cpp`: Admin menu
- `project_tests.cpp`: one GoogleTest file for the project modules
- `Makefile`: build, test, coverage, Valgrind and Cppcheck commands

## Requirements

- C++17-compatible `g++` and `make`
- SQLite development library
- OpenSSL development library
- GoogleTest development library for `make test` and `make coverage`
- `gcov` for coverage reports
- Valgrind and Cppcheck for their optional checks

Do not install packages or modify a managed company server without authorization from IT/team lead.

## Build and run

```bash
make
make run
```

`make` compiles a fresh `student_management` executable each time, avoiding reuse of an old copied binary with stale permissions. If the server filesystem or company security policy blocks execution, contact IT rather than attempting to bypass that restriction.

The program creates `students.db` in the current working directory. Back up an existing database before running an updated build against it.

## Demo credentials for a newly created database

- Admin: `admin` / `Admin@123`
- Student: `student` / `Stud@1234`

Change demonstration credentials before any real use.

## GoogleTest

Run the single test file:

```bash
make test
```

The suite tests database setup and SQL execution, password validation/hashing/login, student operations, attendance, fee installments, reports, and menu/application flows. Test count and results are printed by GoogleTest. The suite must be run on the target environment to verify all tests pass.

## Coverage report

```bash
make coverage
```

This target compiles the application and tests with `--coverage`, runs the tests, then runs GCOV against the generated `project_tests_coverage-*.gcno` files. It prints per-source line coverage for the eight application `.cpp` files and creates detailed `*.gcov` files in the project directory.

Open an individual report, for example:

```bash
nano auth.cpp.gcov
```

In a `.gcov` file, `#####` means an executable line was not reached. The percentage must be read from the `Lines executed` summary. Coverage is not guaranteed by the Makefile or test count; the target is to reach at least 95% across the application source files, and this must be verified from the generated report. If a source file is below target, add tests for its uncovered lines/branches and rerun `make coverage`.

## Valgrind and Cppcheck

```bash
make valgrind
make cppcheck
```

Valgrind runs the console application interactively. Exercise the menus and exit normally to get a useful memory report. Cppcheck performs static analysis. Availability and permission to run these tools depend on the development environment.

## Database and security notes

Existing plaintext accounts are not overwritten during database setup. On their next successful login, the application forces a compliant password reset and stores a PBKDF2-SHA256 hash. Existing student, attendance and fee records are preserved. Existing paid totals are migrated to an opening installment when no installment history exists.

This is an educational project, not a production-ready payment or identity system. Use backups, access controls and secure deployment practices for real use. Do not run migrations on a company/shared database without authorization.

## Static analysis with Cppcheck

Run the project and GoogleTest source files through Cppcheck with warning, performance, and portability checks:

```bash
make cppcheck
```

The target uses C++17, includes the project directory, enables inconclusive diagnostics, and returns a non-zero exit code when Cppcheck reports an enabled finding. Missing system-header diagnostics are suppressed because system include availability differs between development environments. The target does not install tools or change system configuration. If Cppcheck is unavailable on a managed machine, request the approved installation or module from IT.

A successful application build is not proof of a clean Cppcheck run. Record the actual Cppcheck output when reporting static-analysis status.

## Recent validation and presentation updates

- Passwords are entered with terminal echo disabled; an asterisk is shown for each character. When input is redirected (for automated tests), standard input is used without terminal control codes.
- Student marks are stored as `REAL` and the failed-student report uses the strict rule `marks < 35.0` (34 is fail; 35 is pass).
- The admin fee report displays total fee, paid fee, and remaining balance.
- Fee installment timestamps are generated by SQLite using `DEFAULT CURRENT_TIMESTAMP`, based on the server's system clock.
- `make cppcheck` checks the application source files. GoogleTest source is built and run by `make test` / `make coverage`; it is intentionally not passed to Cppcheck because Cppcheck does not expand GoogleTest test macros reliably.

Coverage is measured, not assumed. The coverage percentage depends on which tests execute which application lines. Do not describe it as 100% unless the generated `coverage_summary.txt` confirms 100.00% for the application sources on the target server.


### Password entry
When entering a password in an interactive terminal, the application displays
one `*` for each typed character instead of showing the actual password.
Backspace removes the last character. If the stars do not appear, rebuild the
program with `make clean && make` and run `./student_management`.
