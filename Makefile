# ============================================================================
# Luminix Database Management System - Makefile
# ============================================================================

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -g -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lpthread

# Debug flags
DEBUG_CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -g -O0 -DDEBUG -D_POSIX_C_SOURCE=200809L
DEBUG_LDFLAGS = -lm -lpthread -fsanitize=address -fsanitize=undefined

# Release flags
RELEASE_CFLAGS = -Wall -Wextra -std=c99 -O3 -DNDEBUG -D_POSIX_C_SOURCE=200809L
RELEASE_LDFLAGS = -lm -lpthread

# Coverage flags
COVERAGE_CFLAGS = -Wall -Wextra -std=c99 -g -O0 --coverage
COVERAGE_LDFLAGS = -lm -lpthread --coverage

# Installation paths
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/luminix
DOCDIR = $(PREFIX)/share/doc/luminix
MANDIR = $(PREFIX)/share/man/man1

# Project configuration
TARGET = luminix
VERSION = 1.0.0
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build
DOC_DIR = docs
BIN_DIR = bin
LIB_DIR = lib
INCLUDE_DIR = include
CONFIG_DIR = config
BACKUP_DIR = backups

# Source files
SRC = $(SRC_DIR)/main.c \
      $(SRC_DIR)/database.c \
      $(SRC_DIR)/json_io.c \
      $(SRC_DIR)/utils.c \
      $(SRC_DIR)/index.c \
      $(SRC_DIR)/query_engine.c \
      $(SRC_DIR)/cli.c \
      $(SRC_DIR)/parser.c \
      $(SRC_DIR)/storage.c \
      $(SRC_DIR)/transaction.c \
      $(SRC_DIR)/security.c \
      $(SRC_DIR)/backup.c \
      $(SRC_DIR)/export_import.c

# Header files
HEADERS = $(SRC_DIR)/database.h \
          $(SRC_DIR)/json_io.h \
          $(SRC_DIR)/utils.h \
          $(SRC_DIR)/index.h \
          $(SRC_DIR)/query_engine.h \
          $(SRC_DIR)/cli.h \
          $(SRC_DIR)/parser.h \
          $(SRC_DIR)/storage.h \
          $(SRC_DIR)/transaction.h \
          $(SRC_DIR)/security.h \
          $(SRC_DIR)/backup.h \
          $(SRC_DIR)/export_import.h

# Test files
TEST_SRC = $(TEST_DIR)/test_database.c \
           $(TEST_DIR)/test_utils.c \
           $(TEST_DIR)/test_index.c \
           $(TEST_DIR)/test_parser.c \
           $(TEST_DIR)/test_runner.c

# Object files
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRC))
DEP = $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

# Default target
.PHONY: all
all: directories $(BUILD_DIR)/$(TARGET)

# Debug target
.PHONY: debug
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: LDFLAGS = $(DEBUG_LDFLAGS)
debug: directories $(BUILD_DIR)/$(TARGET)-debug

# Release target
.PHONY: release
release: CFLAGS = $(RELEASE_CFLAGS)
release: LDFLAGS = $(RELEASE_LDFLAGS)
release: directories $(BUILD_DIR)/$(TARGET)-release

# Build directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(INCLUDE_DIR) $(CONFIG_DIR) $(BACKUP_DIR) $(DOC_DIR)

# Main executable
$(BUILD_DIR)/$(TARGET): $(OBJ)
	@echo "Linking $(TARGET)..."
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@cp $@ $(BIN_DIR)/
	@echo "Build complete: $(BIN_DIR)/$(TARGET)"

# Debug executable
$(BUILD_DIR)/$(TARGET)-debug: $(OBJ)
	@echo "Linking $(TARGET)-debug..."
	@$(CC) $(DEBUG_CFLAGS) -o $@ $^ $(DEBUG_LDFLAGS)
	@cp $@ $(BIN_DIR)/
	@echo "Debug build complete: $(BIN_DIR)/$(TARGET)-debug"

# Release executable
$(BUILD_DIR)/$(TARGET)-release: $(OBJ)
	@echo "Linking $(TARGET)-release..."
	@$(CC) $(RELEASE_CFLAGS) -o $@ $^ $(RELEASE_LDFLAGS)
	@cp $@ $(BIN_DIR)/
	@echo "Release build complete: $(BIN_DIR)/$(TARGET)-release"

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Test object files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	@echo "Compiling test $<..."
	@$(CC) $(CFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

# Include dependencies
-include $(DEP)

# ============================================================================
# Test Targets
# ============================================================================

.PHONY: test
test: test-unit test-integration test-performance

.PHONY: test-unit
test-unit: directories $(BUILD_DIR)/test_runner
	@echo "Running unit tests..."
	@./$(BUILD_DIR)/test_runner

$(BUILD_DIR)/test_runner: $(filter-out $(BUILD_DIR)/main.o, $(OBJ)) $(TEST_OBJ)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: test-integration
test-integration: $(BIN_DIR)/$(TARGET)
	@echo "Running integration tests..."
	@python3 $(TEST_DIR)/integration_tests.py

.PHONY: test-performance
test-performance: $(BIN_DIR)/$(TARGET)
	@echo "Running performance tests..."
	@python3 $(TEST_DIR)/performance_tests.py

.PHONY: coverage
coverage: CFLAGS = $(COVERAGE_CFLAGS)
coverage: LDFLAGS = $(COVERAGE_LDFLAGS)
coverage: directories $(BUILD_DIR)/test_runner
	@echo "Running coverage analysis..."
	@./$(BUILD_DIR)/test_runner
	@gcov $(BUILD_DIR)/*.gcno
	@lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info
	@genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage_report
	@echo "Coverage report generated: $(BUILD_DIR)/coverage_report/index.html"

# ============================================================================
# Documentation Targets
# ============================================================================

.PHONY: docs
docs: directories
	@echo "Generating documentation..."
	@doxygen $(CONFIG_DIR)/Doxyfile
	@echo "Documentation generated: $(DOC_DIR)/html/index.html"

.PHONY: man
man:
	@echo "Generating man pages..."
	@mkdir -p $(DOC_DIR)/man
	@pandoc $(DOC_DIR)/luminix.1.md -s -t man -o $(DOC_DIR)/man/luminix.1
	@gzip -f $(DOC_DIR)/man/luminix.1
	@echo "Man page generated: $(DOC_DIR)/man/luminix.1.gz"

# ============================================================================
# Installation Targets
# ============================================================================

.PHONY: install
install: release
	@echo "Installing Luminix $(VERSION)..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(BIN_DIR)/$(TARGET)-release $(DESTDIR)$(BINDIR)/$(TARGET)
	@install -d $(DESTDIR)$(DATADIR)
	@install -m 644 $(CONFIG_DIR)/default.conf $(DESTDIR)$(DATADIR)/
	@install -d $(DESTDIR)$(DOCDIR)
	@install -m 644 README.md LICENSE $(DESTDIR)$(DOCDIR)/
	@install -d $(DESTDIR)$(MANDIR)
	@install -m 644 $(DOC_DIR)/man/luminix.1.gz $(DESTDIR)$(MANDIR)/
	@echo "Luminix $(VERSION) installed successfully!"

.PHONY: install-debug
install-debug: debug
	@echo "Installing Luminix debug version..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(BIN_DIR)/$(TARGET)-debug $(DESTDIR)$(BINDIR)/$(TARGET)-debug
	@echo "Debug version installed successfully!"

.PHONY: uninstall
uninstall:
	@echo "Uninstalling Luminix..."
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)-debug
	@rm -rf $(DESTDIR)$(DATADIR)
	@rm -rf $(DESTDIR)$(DOCDIR)
	@rm -f $(DESTDIR)$(MANDIR)/luminix.1.gz
	@echo "Luminix uninstalled!"

# ============================================================================
# Development Targets
# ============================================================================

.PHONY: run
run: $(BIN_DIR)/$(TARGET)
	@echo "Starting Luminix..."
	@./$(BIN_DIR)/$(TARGET)

.PHONY: run-debug
run-debug: $(BIN_DIR)/$(TARGET)-debug
	@echo "Starting Luminix in debug mode..."
	@./$(BIN_DIR)/$(TARGET)-debug

.PHONY: valgrind
valgrind: debug
	@echo "Running Valgrind memory check..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(BIN_DIR)/$(TARGET)-debug

.PHONY: profile
profile: release
	@echo "Profiling Luminix..."
	@gprof ./$(BIN_DIR)/$(TARGET)-release gmon.out > profile.txt
	@echo "Profile saved to profile.txt"

.PHONY: benchmark
benchmark: release
	@echo "Running benchmarks..."
	@./$(TEST_DIR)/benchmark.sh

.PHONY: format
format:
	@echo "Formatting source code..."
	@find $(SRC_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i
	@find $(TEST_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Formatting complete."

.PHONY: lint
lint:
	@echo "Running static analysis..."
	@cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)
	@echo "Linting complete."

.PHONY: audit
audit:
	@echo "Running security audit..."
	@flawfinder $(SRC_DIR)
	@echo "Security audit complete."

# ============================================================================
# Packaging Targets
# ============================================================================

.PHONY: dist
dist: release docs
	@echo "Creating distribution package..."
	@mkdir -p dist/luminix-$(VERSION)
	@cp -r $(BIN_DIR)/$(TARGET)-release README.md LICENSE $(DOC_DIR)/html $(CONFIG_DIR)/ dist/luminix-$(VERSION)/
	@tar -czf luminix-$(VERSION).tar.gz dist/luminix-$(VERSION)/
	@rm -rf dist/
	@echo "Distribution package created: luminix-$(VERSION).tar.gz"

.PHONY: deb
deb: release
	@echo "Creating Debian package..."
	@mkdir -p debian-package/DEBIAN
	@mkdir -p debian-package$(BINDIR) debian-package$(DATADIR) debian-package$(DOCDIR) debian-package$(MANDIR)
	@cp $(BIN_DIR)/$(TARGET)-release debian-package$(BINDIR)/$(TARGET)
	@cp $(CONFIG_DIR)/default.conf debian-package$(DATADIR)/
	@cp README.md LICENSE debian-package$(DOCDIR)/
	@cp $(DOC_DIR)/man/luminix.1.gz debian-package$(MANDIR)/
	@cp packaging/debian/control debian-package/DEBIAN/
	@dpkg-deb --build debian-package luminix_$(VERSION)_amd64.deb
	@rm -rf debian-package
	@echo "Debian package created: luminix_$(VERSION)_amd64.deb"

.PHONY: rpm
rpm: release
	@echo "Creating RPM package..."
	@mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	@cp luminix-$(VERSION).tar.gz ~/rpmbuild/SOURCES/
	@cp packaging/rpm/luminix.spec ~/rpmbuild/SPECS/
	@rpmbuild -ba ~/rpmbuild/SPECS/luminix.spec
	@echo "RPM package created in ~/rpmbuild/RPMS/"

# ============================================================================
# Database Management Targets
# ============================================================================

.PHONY: backup
backup:
	@echo "Creating database backup..."
	@mkdir -p $(BACKUP_DIR)
	@tar -czf $(BACKUP_DIR)/backup_$(shell date +%Y%m%d_%H%M%S).tar.gz *.json
	@echo "Backup created in $(BACKUP_DIR)/"

.PHONY: restore
restore:
	@echo "Available backups:"
	@ls -lh $(BACKUP_DIR)/*.tar.gz 2>/dev/null || echo "No backups found"
	@echo "Usage: make restore-backup FILE=backup_file.tar.gz"

.PHONY: restore-backup
restore-backup:
	@if [ -z "$(FILE)" ]; then \
		echo "Please specify FILE=backup_file.tar.gz"; \
		exit 1; \
	fi
	@echo "Restoring from $(FILE)..."
	@tar -xzf $(BACKUP_DIR)/$(FILE)
	@echo "Restore complete."

.PHONY: clean-backups
clean-backups:
	@echo "Cleaning old backups (keeping last 10)..."
	@ls -t $(BACKUP_DIR)/*.tar.gz 2>/dev/null | tail -n +11 | xargs rm -f 2>/dev/null || true
	@echo "Backup cleanup complete."

# ============================================================================
# Cleanup Targets
# ============================================================================

.PHONY: clean
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	@rm -f *.gcno *.gcda *.gcov gmon.out profile.txt
	@echo "Clean complete."

.PHONY: clean-all
clean-all: clean
	@echo "Cleaning all generated files..."
	@rm -f database.json test_database.json
	@rm -rf $(DOC_DIR)/html $(DOC_DIR)/man
	@rm -rf $(BACKUP_DIR)
	@rm -f luminix-*.tar.gz luminix_*.deb
	@rm -rf dist debian-package
	@echo "Full clean complete."

.PHONY: distclean
distclean: clean-all uninstall
	@echo "Distribution clean complete."

# ============================================================================
# Help Target
# ============================================================================

.PHONY: help
help:
	@echo "Luminix Database Management System - Makefile Targets"
	@echo ""
	@echo "Build Targets:"
	@echo "  all               Build the default version"
	@echo "  debug             Build with debug symbols and sanitizers"
	@echo "  release           Build optimized release version"
	@echo ""
	@echo "Test Targets:"
	@echo "  test              Run all tests"
	@echo "  test-unit         Run unit tests"
	@echo "  test-integration  Run integration tests"
	@echo "  test-performance  Run performance tests"
	@echo "  coverage          Generate code coverage report"
	@echo ""
	@echo "Documentation:"
	@echo "  docs              Generate Doxygen documentation"
	@echo "  man               Generate man pages"
	@echo ""
	@echo "Installation:"
	@echo "  install           Install to system"
	@echo "  install-debug     Install debug version"
	@echo "  uninstall         Uninstall from system"
	@echo ""
	@echo "Development:"
	@echo "  run               Run the application"
	@echo "  run-debug         Run debug version"
	@echo "  valgrind          Run with Valgrind memory checker"
	@echo "  profile           Generate performance profile"
	@echo "  benchmark         Run benchmarks"
	@echo "  format            Format source code"
	@echo "  lint              Run static analysis"
	@echo "  audit             Run security audit"
	@echo ""
	@echo "Packaging:"
	@echo "  dist              Create distribution tarball"
	@echo "  deb               Create Debian package"
	@echo "  rpm               Create RPM package"
	@echo ""
	@echo "Database Management:"
	@echo "  backup            Create database backup"
	@echo "  restore           List available backups"
	@echo "  restore-backup    Restore from specific backup"
	@echo "  clean-backups     Clean old backups"
	@echo ""
	@echo "Cleanup:"
	@echo "  clean             Clean build files"
	@echo "  clean-all         Clean all generated files"
	@echo "  distclean         Full clean including uninstall"
	@echo ""
	@echo "Miscellaneous:"
	@echo "  help              Show this help message"

# ============================================================================
# Utility Functions
# ============================================================================

# Print version
.PHONY: version
version:
	@echo "Luminix version $(VERSION)"

# Check dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@which $(CC) >/dev/null 2>&1 || echo "Warning: gcc not found"
	@which valgrind >/dev/null 2>&1 || echo "Warning: valgrind not found"
	@which cppcheck >/dev/null 2>&1 || echo "Warning: cppcheck not found"
	@which clang-format >/dev/null 2>&1 || echo "Warning: clang-format not found"
	@which doxygen >/dev/null 2>&1 || echo "Warning: doxygen not found"
	@which lcov >/dev/null 2>&1 || echo "Warning: lcov not found"
	@which gcov >/dev/null 2>&1 || echo "Warning: gcov not found"
	@echo "Dependency check complete."

# Update version
.PHONY: update-version
update-version:
	@if [ -z "$(NEW_VERSION)" ]; then \
		echo "Usage: make update-version NEW_VERSION=x.y.z"; \
		exit 1; \
	fi
	@echo "Updating version from $(VERSION) to $(NEW_VERSION)..."
	@sed -i "s/VERSION = $(VERSION)/VERSION = $(NEW_VERSION)/" Makefile
	@echo "Version updated to $(NEW_VERSION)"