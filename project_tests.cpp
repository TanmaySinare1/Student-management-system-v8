#include <gtest/gtest.h>
#include "database.h"
#include "auth.h"
#include "student.h"
#include "attendance.h"
#include "fees.h"
#include "reports.h"
#include "admin.h"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>

// main.cpp is compiled with -Dmain=studentManagementProgramMain by the Makefile.
int studentManagementProgramMain();
void studentMenu(int userId);

namespace {
class ProjectTest : public ::testing::Test {
protected:
    std::ostringstream output;
    std::streambuf* oldOut = nullptr;

    void SetUp() override {
        ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
        setupDatabase();
        oldOut = std::cout.rdbuf(output.rdbuf());
    }
    void TearDown() override {
        std::cout.rdbuf(oldOut);
        closeDatabase();
    }
    int addTestStudent(const std::string& username = "testuser",
                       const std::string& name = "Test Student",
                       double marks = 80.0) {
        sqlite3_stmt* st = nullptr;
        EXPECT_EQ(sqlite3_prepare_v2(db,
            "INSERT INTO users(username,password,role) VALUES(?,?, 'student')",
            -1, &st, nullptr), SQLITE_OK);
        if (!st) return -1;
        std::string pw = hashPassword("Test@123");
        sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, pw.c_str(), -1, SQLITE_TRANSIENT);
        EXPECT_EQ(sqlite3_step(st), SQLITE_DONE);
        sqlite3_finalize(st);
        int uid = static_cast<int>(sqlite3_last_insert_rowid(db));
        EXPECT_EQ(sqlite3_prepare_v2(db,
            "INSERT INTO students(user_id,name,course,marks) VALUES(?,?, 'Computer Engineering',?)",
            -1, &st, nullptr), SQLITE_OK);
        if (!st) return -1;
        sqlite3_bind_int(st, 1, uid);
        sqlite3_bind_text(st, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 3, marks);
        EXPECT_EQ(sqlite3_step(st), SQLITE_DONE);
        sqlite3_finalize(st);
        int sid = static_cast<int>(sqlite3_last_insert_rowid(db));
        sqlite3_prepare_v2(db, "INSERT INTO attendance(student_id) VALUES(?)", -1, &st, nullptr);
        sqlite3_bind_int(st, 1, sid); sqlite3_step(st); sqlite3_finalize(st);
        sqlite3_prepare_v2(db, "INSERT INTO fees(student_id) VALUES(?)", -1, &st, nullptr);
        sqlite3_bind_int(st, 1, sid); sqlite3_step(st); sqlite3_finalize(st);
        return sid;
    }
    double scalar(const std::string& sql) {
        sqlite3_stmt* st = nullptr; double v = -999;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
            v = sqlite3_column_double(st, 0);
        sqlite3_finalize(st); return v;
    }
    std::string runWithInput(void (*fn)(), const std::string& input) {
        std::istringstream in(input);
        auto* oldIn = std::cin.rdbuf(in.rdbuf());
        fn();
        std::cin.rdbuf(oldIn);
        return output.str();
    }
};

TEST_F(ProjectTest, DatabaseSetupCreatesAllTablesAndDefaultAccounts) {
    EXPECT_GT(scalar("SELECT COUNT(*) FROM users"), 0);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM students"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM attendance"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM fees"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM fee_installments"), 0);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='fee_installments'"), 1);
}
TEST_F(ProjectTest, RunSQLExecutesValidSQLAndReportsInvalidSQL) {
    runSQL("CREATE TABLE sample(value INTEGER);");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM sqlite_master WHERE name='sample'"), 1);
    runSQL("THIS IS NOT SQL;");
    EXPECT_NE(output.str().find("Database error"), std::string::npos);
}
TEST_F(ProjectTest, CloseDatabaseNullsGlobalPointer) {
    closeDatabase();
    EXPECT_EQ(db, nullptr);
    ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    setupDatabase();
}
TEST_F(ProjectTest, PasswordValidationAcceptsAndRejectsBoundaries) {
    EXPECT_TRUE(isValidPassword("Abcd@123"));
    EXPECT_TRUE(isValidPassword("Abcd@1234"));
    EXPECT_TRUE(isValidPassword("Abcd@12345"));
    EXPECT_FALSE(isValidPassword("Ab@123"));
    EXPECT_FALSE(isValidPassword("Abcdefgh"));
    EXPECT_FALSE(isValidPassword("Abcd1234"));
    EXPECT_FALSE(isValidPassword("Abcd@123456"));
    EXPECT_FALSE(isValidPassword(""));
}
TEST_F(ProjectTest, PasswordHashAndVerificationBranches) {
    const auto h = hashPassword("Abcd@123");
    EXPECT_FALSE(h.empty());
    EXPECT_NE(h, "Abcd@123");
    EXPECT_TRUE(verifyPassword("Abcd@123", h));
    EXPECT_FALSE(verifyPassword("Wrong@123", h));
    EXPECT_TRUE(verifyPassword("legacy", "legacy"));
    EXPECT_FALSE(verifyPassword("legacy", "not-legacy"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$bad$data$hash"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$99999$0011$0011"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$1000001$0011$0011"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$210000$abc$0011"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$210000$0011$0011"));
    EXPECT_NE(hashPassword("Abcd@123"), hashPassword("Abcd@123"));
}
TEST_F(ProjectTest, LoginSuccessFailureUnknownUserAndPasswordChange) {
    int uid = -1; std::string role; bool reset = false;
    EXPECT_TRUE(login("admin", "Admin@123", uid, role, reset));
    EXPECT_EQ(role, "admin"); EXPECT_FALSE(reset);
    EXPECT_FALSE(login("admin", "wrong", uid, role, reset));
    EXPECT_FALSE(login("notfound", "wrong", uid, role, reset));
    EXPECT_TRUE(changePassword(uid, "NewPass@1"));
    EXPECT_FALSE(changePassword(uid, "weak"));
    EXPECT_FALSE(changePassword(-999, "NewPass@1"));
    EXPECT_TRUE(login("admin", "NewPass@1", uid, role, reset));
}
TEST_F(ProjectTest, LegacyLoginRequiresPasswordReset) {
    runSQL("INSERT INTO users(username,password,role) VALUES('legacy_user','oldpass','student');");
    int uid = -1; std::string role; bool reset = false;
    EXPECT_TRUE(login("legacy_user", "oldpass", uid, role, reset));
    EXPECT_TRUE(reset);
}
TEST_F(ProjectTest, StudentLookupDisplayAndMissingProfile) {
    const int sid = addTestStudent();
    EXPECT_GT(sid, 0);
    EXPECT_EQ(getStudentId(999999), -1);
    EXPECT_GT(getStudentId(2), 0);
    viewStudentProfile(sid);
    displayStudents();
    viewStudentProfile(-1);
    EXPECT_NE(output.str().find("Test Student"), std::string::npos);
    EXPECT_NE(output.str().find("testuser"), std::string::npos);
    EXPECT_NE(output.str().find("Student profile not found"), std::string::npos);
}
TEST_F(ProjectTest, AddStudentAcceptsValidInputAndCreatesRelatedRows) {
    runWithInput(addStudent, "newuser\nNewPass@1\nNew Student\nComputer Engineering\n91\n");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE username='newuser'"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM students WHERE name='New Student'"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM attendance a JOIN students s ON s.id=a.student_id WHERE s.name='New Student'"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM fees f JOIN students s ON s.id=f.student_id WHERE s.name='New Student'"), 1);
    EXPECT_NE(output.str().find("Student added successfully"), std::string::npos);
}
TEST_F(ProjectTest, AddStudentRetriesWeakPassword) {
    runWithInput(addStudent, "retryuser\nweak\nNewPass@1\nRetry Student\nCSE\n50\n");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM students WHERE name='Retry Student'"), 1);
    EXPECT_NE(output.str().find("Invalid password"), std::string::npos);
}
TEST_F(ProjectTest, AddStudentRejectsInvalidMarksAndDuplicateUsername) {
    runWithInput(addStudent, "badmarks\nNewPass@1\nBad Marks\nCSE\n120\n");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM students WHERE name='Bad Marks'"), 0);
    runWithInput(addStudent, "admin\nNewPass@1\nDuplicate\nCSE\n70\n");
    EXPECT_NE(output.str().find("Could not add account"), std::string::npos);
}
TEST_F(ProjectTest, AttendanceInsertUpdateAndAllViews) {
    const int sid = addTestStudent();
    runWithInput(updateAttendance, std::to_string(sid) + "\n10\n8\n");
    EXPECT_EQ(scalar("SELECT attended FROM attendance WHERE student_id=" + std::to_string(sid)), 8);
    runWithInput(updateAttendance, std::to_string(sid) + "\n20\n15\n");
    EXPECT_EQ(scalar("SELECT total FROM attendance WHERE student_id=" + std::to_string(sid)), 20);
    viewStudentAttendance(sid); viewAllAttendance();
    EXPECT_NE(output.str().find("75"), std::string::npos);
}
TEST_F(ProjectTest, AttendanceRejectsInvalidMissingAndZeroTotalCases) {
    const int sid = addTestStudent();
    runWithInput(updateAttendance, "-1\n5\n2\n");
    runWithInput(updateAttendance, std::to_string(sid) + "\n-1\n0\n");
    runWithInput(updateAttendance, std::to_string(sid) + "\n5\n6\n");
    runWithInput(updateAttendance, "99999\n5\n2\n");
    viewStudentAttendance(99999);
    EXPECT_NE(output.str().find("Invalid attendance values"), std::string::npos);
    EXPECT_NE(output.str().find("Student ID not found"), std::string::npos);
    EXPECT_NE(output.str().find("Attendance details not available"), std::string::npos);
}
TEST_F(ProjectTest, FeeSetPaymentBalanceHistoryAndFullPayment) {
    const int sid = addTestStudent();
    runWithInput(updateFee, std::to_string(sid) + "\n1000\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n250\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n750\n");
    EXPECT_DOUBLE_EQ(scalar("SELECT paid_fee FROM fees WHERE student_id=" + std::to_string(sid)), 1000);
    viewStudentFee(sid); viewAllFees();
    runWithInput(viewFeeInstallments, std::to_string(sid) + "\n");
    EXPECT_NE(output.str().find("Installment recorded successfully"), std::string::npos);
    EXPECT_NE(output.str().find("Status: NIL"), std::string::npos);
    EXPECT_NE(output.str().find("Total installments recorded: 1000.00"), std::string::npos);
}
TEST_F(ProjectTest, FeeFunctionsCoverInvalidMissingAndExcessValues) {
    const int sid = addTestStudent();
    runWithInput(updateFee, "-1\n100\n");
    runWithInput(updateFee, std::to_string(sid) + "\n-5\n");
    runWithInput(updateFee, "99999\n100\n");
    runWithInput(updateFee, std::to_string(sid) + "\n100\n");
    runWithInput(addFeeInstallment, "-1\n10\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n0\n");
    runWithInput(addFeeInstallment, "99999\n10\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n150\n");
    runWithInput(viewFeeInstallments, "-1\n");
    viewStudentFee(99999);
    EXPECT_NE(output.str().find("Invalid student ID or fee amount"), std::string::npos);
    EXPECT_NE(output.str().find("Installment exceeds remaining balance"), std::string::npos);
    EXPECT_NE(output.str().find("Student fee record not found"), std::string::npos);
    EXPECT_NE(output.str().find("Fee details not found"), std::string::npos);
}
TEST_F(ProjectTest, FeeHistoryShowsEmptyHistory) {
    const int sid = addTestStudent();
    runWithInput(viewFeeInstallments, std::to_string(sid) + "\n");
    EXPECT_NE(output.str().find("No installment payments recorded"), std::string::npos);
}
TEST_F(ProjectTest, FailedStudentReportShowsOnlyBelow35AndEmptyCase) {
    addTestStudent("failuser", "Failing Student", 30);
    addTestStudent("passuser", "Passing Student", 70);
    viewFailedStudents();
    EXPECT_NE(output.str().find("Failing Student"), std::string::npos);
    EXPECT_EQ(output.str().find("Passing Student"), std::string::npos);
}
TEST_F(ProjectTest, StudentMenuRunsAllOptionsAndLogout) {
    const int userId = 2; // seeded student account
    std::istringstream in("1\n2\n3\n9\n0\n");
    auto* oldIn = std::cin.rdbuf(in.rdbuf());
    studentMenu(userId);
    std::cin.rdbuf(oldIn);
    EXPECT_NE(output.str().find("MY PROFILE"), std::string::npos);
    EXPECT_NE(output.str().find("MY ATTENDANCE"), std::string::npos);
    EXPECT_NE(output.str().find("FEE DETAILS"), std::string::npos);
    EXPECT_NE(output.str().find("Invalid choice"), std::string::npos);
    EXPECT_NE(output.str().find("Student logged out"), std::string::npos);
}
TEST_F(ProjectTest, StudentMenuHandlesMissingStudentProfile) {
    studentMenu(999999);
    EXPECT_NE(output.str().find("Student profile not found"), std::string::npos);
}
TEST_F(ProjectTest, MainInitializesDatabaseCoversLoginRoutingAndExits) {
    closeDatabase(); // main() opens its own on-disk database.
    const auto original = std::filesystem::current_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = std::filesystem::temp_directory_path() / ("sms_gtest_" + std::to_string(stamp));
    std::filesystem::create_directories(temp);
    std::filesystem::current_path(temp);
    // Invalid menu choice, failed login, wrong login role, valid student login,
    // valid admin login, invalid admin menu choice, and clean application exit.
    std::istringstream in(
        "99\n"
        "1\nadmin\nwrong\n"
        "1\nstudent\nStud@1234\n"
        "2\nstudent\nStud@1234\n0\n"
        "1\nadmin\nAdmin@123\n99\n0\n"
        "0\n");
    auto* oldIn = std::cin.rdbuf(in.rdbuf());
    const int result = studentManagementProgramMain();
    std::cin.rdbuf(oldIn);
    std::filesystem::current_path(original);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(std::filesystem::exists(temp / "students.db"));
    EXPECT_NE(output.str().find("Invalid choice"), std::string::npos);
    EXPECT_NE(output.str().find("Invalid username or password"), std::string::npos);
    EXPECT_NE(output.str().find("This account cannot use that login type"), std::string::npos);
    EXPECT_NE(output.str().find("Student logged out"), std::string::npos);
    EXPECT_NE(output.str().find("Admin logged out"), std::string::npos);
    EXPECT_NE(output.str().find("Program ended"), std::string::npos);
    std::filesystem::remove_all(temp);
}

TEST_F(ProjectTest, OpenDatabaseSucceedsInWritableWorkingDirectory) {
    closeDatabase();
    const auto original = std::filesystem::current_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = std::filesystem::temp_directory_path() / ("sms_open_ok_" + std::to_string(stamp));
    std::filesystem::create_directories(temp);
    std::filesystem::current_path(temp);
    EXPECT_TRUE(openDatabase());
    EXPECT_NE(db, nullptr);
    closeDatabase();
    std::filesystem::current_path(original);
    EXPECT_TRUE(std::filesystem::exists(temp / "students.db"));
    std::filesystem::remove_all(temp);
}

TEST_F(ProjectTest, OpenDatabaseReportsFailureForDatabaseDirectoryPath) {
    closeDatabase();
    const auto original = std::filesystem::current_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = std::filesystem::temp_directory_path() / ("sms_open_fail_" + std::to_string(stamp));
    std::filesystem::create_directories(temp / "students.db");
    std::filesystem::current_path(temp);
    EXPECT_FALSE(openDatabase());
    closeDatabase();
    std::filesystem::current_path(original);
    EXPECT_NE(output.str().find("Cannot open database"), std::string::npos);
    std::filesystem::remove_all(temp);
}

TEST_F(ProjectTest, DatabaseSetupPreservesExistingUsersAndMigratesOpeningFee) {
    const int sid = addTestStudent("openingfee", "Opening Fee Student", 40);
    runSQL("UPDATE fees SET total_fee=900, paid_fee=125 WHERE student_id=" + std::to_string(sid) + ";");
    setupDatabase(); // existing account is retained; paid balance is migrated once.
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE username='admin'"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM fee_installments WHERE student_id=" + std::to_string(sid)), 1);
    EXPECT_DOUBLE_EQ(scalar("SELECT amount FROM fee_installments WHERE student_id=" + std::to_string(sid)), 125);
    setupDatabase();
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM fee_installments WHERE student_id=" + std::to_string(sid)), 1);
}

TEST_F(ProjectTest, MainForcesLegacyPasswordResetAndRetriesInvalidNewPassword) {
    closeDatabase();
    const auto original = std::filesystem::current_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = std::filesystem::temp_directory_path() / ("sms_legacy_main_" + std::to_string(stamp));
    std::filesystem::create_directories(temp);
    sqlite3* legacyDb = nullptr;
    ASSERT_EQ(sqlite3_open((temp / "students.db").string().c_str(), &legacyDb), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(legacyDb,
        "CREATE TABLE users(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT UNIQUE NOT NULL,password TEXT NOT NULL,role TEXT NOT NULL);"
        "INSERT INTO users(username,password,role) VALUES('legacy_main','oldpass','student');",
        nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(legacyDb);
    std::filesystem::current_path(temp);
    std::istringstream in("1\nlegacy_main\noldpass\nweak\nNewPass@1\n0\n");
    auto* oldIn = std::cin.rdbuf(in.rdbuf());
    const int result = studentManagementProgramMain();
    std::cin.rdbuf(oldIn);
    std::filesystem::current_path(original);
    EXPECT_EQ(result, 0);
    EXPECT_NE(output.str().find("old or weak password"), std::string::npos);
    EXPECT_NE(output.str().find("Invalid password"), std::string::npos);
    EXPECT_NE(output.str().find("Password updated and securely hashed"), std::string::npos);
    std::filesystem::remove_all(temp);
}

TEST_F(ProjectTest, PasswordVerificationRejectsMissingFieldsAndInvalidHex) {
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$210000"));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$210000$zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz$" + std::string(64, '0')));
    EXPECT_FALSE(verifyPassword("x", "pbkdf2_sha256$210000$" + std::string(32, '0') + "$" + std::string(64, 'z')));
}

TEST_F(ProjectTest, StudentCreationRejectsNegativeMarksAndProfileReportCanBeEmpty) {
    runWithInput(addStudent, "negativeMarks\nNewPass@1\nNegative Marks\nCSE\n-1\n");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM students WHERE name='Negative Marks'"), 0);
    viewFailedStudents();
    EXPECT_NE(output.str().find("Marks must be between 0 and 100"), std::string::npos);
    EXPECT_NE(output.str().find("No students below 35%"), std::string::npos);
}

TEST_F(ProjectTest, StudentInsertFailureReportsProfileSaveError) {
    runSQL("CREATE TRIGGER reject_student BEFORE INSERT ON students BEGIN SELECT RAISE(ABORT,'forced test failure'); END;");
    runWithInput(addStudent, "triggeruser\nNewPass@1\nTrigger Student\nCSE\n50\n");
    EXPECT_NE(output.str().find("Could not save student profile"), std::string::npos);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE username='triggeruser'"), 1);
}

TEST_F(ProjectTest, AttendanceZeroTotalAndMissingFeeHistoryPaths) {
    const int sid = addTestStudent("zerocase", "Zero Case Student", 50);
    runWithInput(updateAttendance, std::to_string(sid) + "\n0\n0\n");
    viewStudentAttendance(sid);
    viewAllAttendance();
    runWithInput(viewFeeInstallments, "999999\n");
    EXPECT_NE(output.str().find("Attendance percentage: 0"), std::string::npos);
    EXPECT_NE(output.str().find("No installment payments recorded"), std::string::npos);
}
TEST_F(ProjectTest, AdminMenuExercisesEveryActionAndLogout) {
    const int sid = addTestStudent("menuuser", "Menu Student", 20);
    std::ostringstream inText;
    inText << "1\nnewmenu\nNewPass@1\nMenu Added\nCSE\n60\n"
           << "2\n3\n" << sid << "\n10\n8\n"
           << "4\n5\n6\n" << sid << "\n500\n"
           << "7\n" << sid << "\n100\n8\n9\n" << sid << "\n99\n0\n";
    std::istringstream in(inText.str());
    auto* oldIn = std::cin.rdbuf(in.rdbuf());
    adminMenu();
    std::cin.rdbuf(oldIn);
    EXPECT_NE(output.str().find("Student added successfully"), std::string::npos);
    EXPECT_NE(output.str().find("Attendance updated successfully"), std::string::npos);
    EXPECT_NE(output.str().find("STUDENTS BELOW 35%"), std::string::npos);
    EXPECT_NE(output.str().find("Installment recorded successfully"), std::string::npos);
    EXPECT_NE(output.str().find("Admin logged out"), std::string::npos);
}
TEST_F(ProjectTest, AdminMenuHandlesInvalidChoice) {
    runWithInput(adminMenu, "99\n0\n");
    EXPECT_NE(output.str().find("Invalid choice"), std::string::npos);
}

TEST_F(ProjectTest, MarksBoundaryThirtyFourFailsAndThirtyFivePasses) {
    addTestStudent("mark34", "Thirty Four", 34.0);
    addTestStudent("mark35", "Thirty Five", 35.0);
    viewFailedStudents();
    EXPECT_NE(output.str().find("Thirty Four"), std::string::npos);
    EXPECT_EQ(output.str().find("Thirty Five"), std::string::npos);
}

TEST_F(ProjectTest, AdminFeeReportShowsRemainingBalanceAndSystemTimestamp) {
    const int sid = addTestStudent("balanceuser", "Balance Student", 65);
    runWithInput(updateFee, std::to_string(sid) + "\n1000\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n250\n");
    viewAllFees();
    EXPECT_NE(output.str().find("Balance: 750.00"), std::string::npos);
    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db,
        "SELECT payment_date FROM fee_installments WHERE student_id=?", -1,
        &statement, nullptr), SQLITE_OK);
    sqlite3_bind_int(statement, 1, sid);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    const unsigned char* timestamp = sqlite3_column_text(statement, 0);
    ASSERT_NE(timestamp, nullptr);
    EXPECT_GE(std::string(reinterpret_cast<const char*>(timestamp)).size(), 19U);
    sqlite3_finalize(statement);
}

TEST_F(ProjectTest, FeeUpdateCannotReduceTotalBelowPaidAmount) {
    const int sid = addTestStudent("paiduser", "Paid Student", 70);
    runWithInput(updateFee, std::to_string(sid) + "\n500\n");
    runWithInput(addFeeInstallment, std::to_string(sid) + "\n300\n");
    runWithInput(updateFee, std::to_string(sid) + "\n200\n");
    EXPECT_DOUBLE_EQ(scalar("SELECT total_fee FROM fees WHERE student_id=" + std::to_string(sid)), 500);
    EXPECT_NE(output.str().find("ensure total fee is not below amount already paid"), std::string::npos);
}
}
