# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/coverage/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@
