# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

$(BUILD_DIR)/full/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(full_CFLAGS) -c $< -o $@
$(BUILD_DIR)/coverage/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(coverage_CFLAGS) -c $< -o $@
