CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O0
LDLIBS := -lsqlite3 -lcrypto
APP := student_management
CORE_SOURCES := database.cpp auth.cpp student.cpp attendance.cpp fees.cpp reports.cpp admin.cpp
SOURCES := main.cpp $(CORE_SOURCES)
TEST_BIN := project_tests
COVERAGE_BIN := project_tests_coverage

# The server may not provide a gtest.pc file. Link the standard libraries directly.
GTEST_CFLAGS := $(shell pkg-config --cflags gtest 2>/dev/null)
GTEST_LIBS := -lgtest_main -lgtest -pthread
CPPCHECK ?= cppcheck
CPPCHECK_FLAGS := --enable=warning,performance,portability --inconclusive --error-exitcode=1 --std=c++17 --suppress=missingIncludeSystem --inline-suppr -I.

.PHONY: all run clean test coverage valgrind cppcheck tools

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDLIBS) -o $(APP)

run: all
	./$(APP)

test:
	$(CXX) $(CXXFLAGS) $(GTEST_CFLAGS) -I. -Dmain=studentManagementProgramMain project_tests.cpp $(SOURCES) $(LDLIBS) $(GTEST_LIBS) -o $(TEST_BIN)
	./$(TEST_BIN)

coverage:
	rm -f $(COVERAGE_BIN) $(COVERAGE_BIN)-*.gcda $(COVERAGE_BIN)-*.gcno *.gcov
	$(CXX) $(CXXFLAGS) --coverage $(GTEST_CFLAGS) -I. -Dmain=studentManagementProgramMain project_tests.cpp $(SOURCES) $(LDLIBS) $(GTEST_LIBS) --coverage -o $(COVERAGE_BIN)
	./$(COVERAGE_BIN)
	gcov -b -c $(COVERAGE_BIN)-*.gcno | tee coverage_summary.txt
	@awk '/^File / { file=substr($$0,7,length($$0)-7) } /^Lines executed:/ { if (file ~ /^(main|database|auth|student|attendance|fees|reports|admin)\.cpp$$/) { pct=$$2; sub(/^executed:/, "", pct); sub(/%$$/, "", pct); n=$$4; covered += int((pct*n/100)+0.5); total += n; print file ": " $$0 } } END { if (total > 0) printf "OVERALL APPLICATION LINE COVERAGE: %.2f%% (%d/%d lines)\n", 100*covered/total, covered, total; else print "No application source coverage records found." }' coverage_summary.txt
	@echo "Full per-line reports are the *.gcov files in this directory."

valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./$(APP)

cppcheck:
	@command -v $(CPPCHECK) >/dev/null 2>&1 || { echo "Cppcheck is not installed or not on PATH. Ask IT to provide the approved tool; no installation is attempted."; exit 127; }
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(SOURCES)

tools:
	@command -v g++ >/dev/null || (echo "g++ missing"; exit 1)
	@command -v make >/dev/null || (echo "make missing"; exit 1)
	@command -v gcov >/dev/null || echo "gcov missing"
	@command -v valgrind >/dev/null || echo "Valgrind missing"
	@command -v cppcheck >/dev/null || echo "Cppcheck missing"
	@pkg-config --exists gtest || echo "GoogleTest pkg-config missing; direct -lgtest linking will be attempted"

clean:
	rm -f $(APP) $(TEST_BIN) $(COVERAGE_BIN) $(COVERAGE_BIN)-*.gcda $(COVERAGE_BIN)-*.gcno *.gcda *.gcno *.gcov
