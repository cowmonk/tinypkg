# TinyPkg Modular Makefile
# Build system for the TinyPkg source-based package manager

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -D_GNU_SOURCE -fPIC
DEBUG_CFLAGS = -g -DDEBUG -O0 -fsanitize=address -fno-omit-frame-pointer
INCLUDES = -Iinclude
LIBS = -lcurl -lgit2 -ljansson -lcrypto -lssl -lz -lpthread
PKG_CONFIG_LIBS = $(shell pkg-config --libs libcurl libgit2 jansson)

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = build/obj
BINDIR = build/bin
TESTDIR = tests
DOCDIR = docs
SCRIPTDIR = scripts
CONFIGDIR = config
EXAMPLEDIR = examples

# Target binary
TARGET = tinypkg
TARGET_PATH = $(BINDIR)/$(TARGET)

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
HEADERS = $(wildcard $(INCDIR)/*.h) $(wildcard $(SRCDIR)/*.h)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(OBJDIR)/test_%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/%)

# Installation paths
PREFIX = /usr
BINDIR_INSTALL = $(PREFIX)/bin
SYSCONFDIR = /etc
LOCALSTATEDIR = /var
MANDIR = $(PREFIX)/share/man

# Version information
VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION_PATCH = 0
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Build configuration
BUILD_DATE = $(shell date -u +"%Y%m%d_%H%M%S")
GIT_HASH = $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_USER = $(shell whoami)
BUILD_HOST = $(shell uname -n 2>/dev/null || echo "unknown")

# Define version macros
VERSION_DEFINES = -DTINYPKG_VERSION=\"$(VERSION)\" \
                 -DTINYPKG_BUILD_DATE=\"$(BUILD_DATE)\" \
                 -DTINYPKG_GIT_HASH=\"$(GIT_HASH)\" \
                 -DTINYPKG_BUILD_USER=\"$(BUILD_USER)\" \
                 -DTINYPKG_BUILD_HOST=\"$(BUILD_HOST)\"

# Color output
COLOR_RED = \033[31m
COLOR_GREEN = \033[32m
COLOR_YELLOW = \033[33m
COLOR_BLUE = \033[34m
COLOR_RESET = \033[0m

.PHONY: all clean install uninstall check-deps debug release test docs dist help

# Default target
all: check-deps $(TARGET_PATH)

# Create build directories
$(OBJDIR) $(BINDIR):
	@printf "$(COLOR_BLUE)[CREATE]$(COLOR_RESET) Creating directory $@\n"
	@mkdir -p $@

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(OBJDIR)
	@printf "$(COLOR_GREEN)[COMPILE]$(COLOR_RESET) $<\n"
	@$(CC) $(CFLAGS) $(INCLUDES) $(VERSION_DEFINES) -c $< -o $@

# Link main executable
$(TARGET_PATH): $(OBJECTS) | $(BINDIR)
	@printf "$(COLOR_YELLOW)[LINK]$(COLOR_RESET) $@\n"
	@$(CC) $(OBJECTS) $(LIBS) -o $@
	@printf "$(COLOR_GREEN)[SUCCESS]$(COLOR_RESET) Build completed: $@\n"

# Debug build
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: clean $(TARGET_PATH)
	@printf "$(COLOR_YELLOW)[DEBUG]$(COLOR_RESET) Debug build completed\n"

# Release build with optimizations
release: CFLAGS += -O3 -DNDEBUG -march=native
release: clean $(TARGET_PATH)
	@strip $(TARGET_PATH)
	@printf "$(COLOR_GREEN)[RELEASE]$(COLOR_RESET) Release build completed\n"

# Check dependencies
check-deps:
	@printf "$(COLOR_BLUE)[CHECK]$(COLOR_RESET) Checking dependencies...\n"
	@command -v pkg-config >/dev/null 2>&1 || { \
		printf "$(COLOR_RED)[ERROR]$(COLOR_RESET) pkg-config not found\n"; exit 1; }
	@pkg-config --exists libcurl || { \
		printf "$(COLOR_RED)[ERROR]$(COLOR_RESET) libcurl-dev not found\n"; exit 1; }
	@pkg-config --exists libgit2 || { \
		printf "$(COLOR_RED)[ERROR]$(COLOR_RESET) libgit2-dev not found\n"; exit 1; }
	@pkg-config --exists jansson || { \
		printf "$(COLOR_RED)[ERROR]$(COLOR_RESET) libjansson-dev not found\n"; exit 1; }
	@printf "$(COLOR_GREEN)[OK]$(COLOR_RESET) All dependencies found\n"

# Compile tests
$(OBJDIR)/test_%.o: $(TESTDIR)/%.c $(HEADERS) | $(OBJDIR)
	@printf "$(COLOR_BLUE)[TEST-COMPILE]$(COLOR_RESET) $<\n"
	@$(CC) $(CFLAGS) $(INCLUDES) $(VERSION_DEFINES) -c $< -o $@

# Link test executables
$(BINDIR)/test_%: $(OBJDIR)/test_%.o $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | $(BINDIR)
	@printf "$(COLOR_BLUE)[TEST-LINK]$(COLOR_RESET) $@\n"
	@$(CC) $^ $(LIBS) -o $@

# Build and run tests
test: $(TEST_TARGETS)
	@printf "$(COLOR_BLUE)[TEST]$(COLOR_RESET) Running test suite...\n"
	@passed=0; total=0; \
	for test in $(TEST_TARGETS); do \
		total=$$((total + 1)); \
		printf "$(COLOR_BLUE)[RUN]$(COLOR_RESET) $$test\n"; \
		if $$test; then \
			printf "$(COLOR_GREEN)[PASS]$(COLOR_RESET) $$test\n"; \
			passed=$$((passed + 1)); \
		else \
			printf "$(COLOR_RED)[FAIL]$(COLOR_RESET) $$test\n"; \
		fi; \
	done; \
	printf "$(COLOR_BLUE)[RESULT]$(COLOR_RESET) $$passed/$$total tests passed\n"; \
	if [ $$passed -eq $$total ]; then \
		printf "$(COLOR_GREEN)[SUCCESS]$(COLOR_RESET) All tests passed!\n"; \
		exit 0; \
	else \
		printf "$(COLOR_RED)[FAILURE]$(COLOR_RESET) Some tests failed!\n"; \
		exit 1; \
	fi

# Install system-wide
install: $(TARGET_PATH)
	@printf "$(COLOR_BLUE)[INSTALL]$(COLOR_RESET) Installing TinyPkg...\n"
	@install -D -m 755 $(TARGET_PATH) $(DESTDIR)$(BINDIR_INSTALL)/$(TARGET)
	@printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) Binary installed to $(BINDIR_INSTALL)/$(TARGET)\n"
	
	# Create system directories
	@install -d -m 755 $(DESTDIR)$(SYSCONFDIR)/tinypkg
	@install -d -m 755 $(DESTDIR)$(LOCALSTATEDIR)/cache/tinypkg
	@install -d -m 755 $(DESTDIR)$(LOCALSTATEDIR)/lib/tinypkg
	@install -d -m 755 $(DESTDIR)$(LOCALSTATEDIR)/lib/tinypkg/repo
	@install -d -m 755 $(DESTDIR)$(LOCALSTATEDIR)/log/tinypkg
	@printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) System directories created\n"
	
	# Install configuration files
	@if [ -f $(CONFIGDIR)/tinypkg.conf.example ]; then \
		install -D -m 644 $(CONFIGDIR)/tinypkg.conf.example $(DESTDIR)$(SYSCONFDIR)/tinypkg/tinypkg.conf.example; \
		printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) Configuration examples installed\n"; \
	fi
	
	# Install systemd service files
	@if [ -d $(CONFIGDIR)/systemd ]; then \
		install -D -m 644 $(CONFIGDIR)/systemd/tinypkg-update.service $(DESTDIR)/etc/systemd/system/; \
		install -D -m 644 $(CONFIGDIR)/systemd/tinypkg-update.timer $(DESTDIR)/etc/systemd/system/; \
		printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) Systemd services installed\n"; \
	fi
	
	# Install bash completion
	@if [ -f contrib/bash-completion ]; then \
		install -D -m 644 contrib/bash-completion $(DESTDIR)/etc/bash_completion.d/tinypkg; \
		printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) Bash completion installed\n"; \
	fi
	
	# Install man page
	@if [ -f $(DOCDIR)/tinypkg.1 ]; then \
		install -D -m 644 $(DOCDIR)/tinypkg.1 $(DESTDIR)$(MANDIR)/man1/tinypkg.1; \
		printf "$(COLOR_GREEN)[INSTALL]$(COLOR_RESET) Man page installed\n"; \
	fi
	
	@printf "$(COLOR_GREEN)[SUCCESS]$(COLOR_RESET) TinyPkg installation completed!\n"
	@printf "$(COLOR_BLUE)[INFO]$(COLOR_RESET) Run 'sudo tinypkg -s' to sync repositories\n"

# Uninstall
uninstall:
	@printf "$(COLOR_BLUE)[UNINSTALL]$(COLOR_RESET) Removing TinyPkg...\n"
	@rm -f $(DESTDIR)$(BINDIR_INSTALL)/$(TARGET)
	@rm -f $(DESTDIR)/etc/bash_completion.d/tinypkg
	@rm -f $(DESTDIR)$(MANDIR)/man1/tinypkg.1
	@rm -f $(DESTDIR)/etc/systemd/system/tinypkg-update.service
	@rm -f $(DESTDIR)/etc/systemd/system/tinypkg-update.timer
	@printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) Configuration and data directories preserved\n"
	@printf "$(COLOR_GREEN)[SUCCESS]$(COLOR_RESET) TinyPkg uninstalled\n"

# Generate documentation
docs:
	@printf "$(COLOR_BLUE)[DOCS]$(COLOR_RESET) Generating documentation...\n"
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
		printf "$(COLOR_GREEN)[DOCS]$(COLOR_RESET) API documentation generated\n"; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) doxygen not found, skipping API docs\n"; \
	fi
	@if command -v pandoc >/dev/null 2>&1; then \
		pandoc $(DOCDIR)/README.md -o $(DOCDIR)/tinypkg.1; \
		printf "$(COLOR_GREEN)[DOCS]$(COLOR_RESET) Man page generated\n"; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) pandoc not found, skipping man page\n"; \
	fi

# Create distribution package
dist: clean
	@printf "$(COLOR_BLUE)[DIST]$(COLOR_RESET) Creating distribution package...\n"
	@mkdir -p dist
	@tar -czf dist/tinypkg-$(VERSION).tar.gz \
		--exclude-from=.gitignore \
		--exclude=dist \
		--exclude=build \
		--exclude=.git \
		--transform 's,^,tinypkg-$(VERSION)/,' \
		*
	@printf "$(COLOR_GREEN)[DIST]$(COLOR_RESET) Package created: dist/tinypkg-$(VERSION).tar.gz\n"

# Clean build artifacts
clean:
	@printf "$(COLOR_BLUE)[CLEAN]$(COLOR_RESET) Removing build artifacts...\n"
	@rm -rf build/
	@rm -f core core.*
	@printf "$(COLOR_GREEN)[CLEAN]$(COLOR_RESET) Build artifacts removed\n"

# Deep clean (including generated files)
distclean: clean
	@printf "$(COLOR_BLUE)[DISTCLEAN]$(COLOR_RESET) Deep cleaning...\n"
	@rm -rf dist/
	@rm -rf docs/html/
	@rm -f $(DOCDIR)/tinypkg.1
	@printf "$(COLOR_GREEN)[DISTCLEAN]$(COLOR_RESET) Deep clean completed\n"

# Static analysis
analyze:
	@printf "$(COLOR_BLUE)[ANALYZE]$(COLOR_RESET) Running static analysis...\n"
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c99 $(SRCDIR)/ $(INCDIR)/; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) cppcheck not found\n"; \
	fi
	@if command -v clang-analyzer >/dev/null 2>&1; then \
		scan-build make; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) clang static analyzer not found\n"; \
	fi

# Format code
format:
	@printf "$(COLOR_BLUE)[FORMAT]$(COLOR_RESET) Formatting source code...\n"
	@if command -v clang-format >/dev/null 2>&1; then \
		find $(SRCDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		printf "$(COLOR_GREEN)[FORMAT]$(COLOR_RESET) Code formatted\n"; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) clang-format not found\n"; \
	fi

# Performance profiling
profile: CFLAGS += -pg -O2
profile: clean $(TARGET_PATH)
	@printf "$(COLOR_BLUE)[PROFILE]$(COLOR_RESET) Profiling build completed\n"
	@printf "$(COLOR_BLUE)[INFO]$(COLOR_RESET) Run gprof $(TARGET_PATH) gmon.out after execution\n"

# Memory check
memcheck: debug
	@printf "$(COLOR_BLUE)[MEMCHECK]$(COLOR_RESET) Running memory check...\n"
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(TARGET_PATH) --help; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) valgrind not found\n"; \
	fi

# Show build information
info:
	@printf "$(COLOR_BLUE)[INFO]$(COLOR_RESET) TinyPkg Build Information\n"
	@printf "Version: $(VERSION)\n"
	@printf "Build Date: $(BUILD_DATE)\n"
	@printf "Git Hash: $(GIT_HASH)\n"
	@printf "Build User: $(BUILD_USER)@$(BUILD_HOST)\n"
	@printf "Compiler: $(CC) $(CFLAGS)\n"
	@printf "Libraries: $(LIBS)\n"
	@printf "Sources: $(words $(SOURCES)) files\n"
	@printf "Headers: $(words $(HEADERS)) files\n"
	@printf "Tests: $(words $(TEST_SOURCES)) files\n"

# Development setup
dev-setup:
	@printf "$(COLOR_BLUE)[DEV-SETUP]$(COLOR_RESET) Setting up development environment...\n"
	@if [ -f $(SCRIPTDIR)/setup-dev.sh ]; then \
		bash $(SCRIPTDIR)/setup-dev.sh; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) Development setup script not found\n"; \
	fi

# Create example package repository
examples:
	@printf "$(COLOR_BLUE)[EXAMPLES]$(COLOR_RESET) Creating example packages...\n"
	@if [ -f $(SCRIPTDIR)/package-examples.sh ]; then \
		bash $(SCRIPTDIR)/package-examples.sh; \
	else \
		printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) Package examples script not found\n"; \
	fi

# Help target
help:
	@printf "$(COLOR_BLUE)TinyPkg Build System$(COLOR_RESET)\n"
	@printf "Available targets:\n\n"
	@printf "  $(COLOR_GREEN)all$(COLOR_RESET)         - Build TinyPkg (default)\n"
	@printf "  $(COLOR_GREEN)debug$(COLOR_RESET)       - Build with debug symbols and AddressSanitizer\n"
	@printf "  $(COLOR_GREEN)release$(COLOR_RESET)     - Build optimized release version\n"
	@printf "  $(COLOR_GREEN)test$(COLOR_RESET)        - Build and run test suite\n"
	@printf "  $(COLOR_GREEN)install$(COLOR_RESET)     - Install TinyPkg system-wide\n"
	@printf "  $(COLOR_GREEN)uninstall$(COLOR_RESET)   - Remove TinyPkg from system\n"
	@printf "  $(COLOR_GREEN)clean$(COLOR_RESET)       - Remove build artifacts\n"
	@printf "  $(COLOR_GREEN)distclean$(COLOR_RESET)   - Deep clean including generated files\n"
	@printf "  $(COLOR_GREEN)docs$(COLOR_RESET)        - Generate documentation\n"
	@printf "  $(COLOR_GREEN)dist$(COLOR_RESET)        - Create distribution package\n"
	@printf "  $(COLOR_GREEN)check-deps$(COLOR_RESET)  - Check for required dependencies\n"
	@printf "  $(COLOR_GREEN)analyze$(COLOR_RESET)     - Run static code analysis\n"
	@printf "  $(COLOR_GREEN)format$(COLOR_RESET)      - Format source code\n"
	@printf "  $(COLOR_GREEN)profile$(COLOR_RESET)     - Build with profiling support\n"
	@printf "  $(COLOR_GREEN)memcheck$(COLOR_RESET)    - Run memory checker\n"
	@printf "  $(COLOR_GREEN)info$(COLOR_RESET)        - Show build information\n"
	@printf "  $(COLOR_GREEN)dev-setup$(COLOR_RESET)   - Set up development environment\n"
	@printf "  $(COLOR_GREEN)examples$(COLOR_RESET)    - Create example packages\n"
	@printf "  $(COLOR_GREEN)help$(COLOR_RESET)        - Show this help message\n"
	@printf "\nExample usage:\n"
	@printf "  make debug              # Debug build\n"
	@printf "  make test               # Run tests\n"
	@printf "  sudo make install       # Install system-wide\n"
	@printf "  make DESTDIR=/tmp install # Install to staging directory\n"

# Include dependency files if they exist
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJDIR)/%.d: $(SRCDIR)/%.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@
