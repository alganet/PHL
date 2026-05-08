# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

$(BUILD_DIR)/$(MODE)/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(MODE_CFLAGS) -MMD -MP -c $< -o $@

# Auto-generated header dependencies (from -MMD -MP).
# Each .d file lists the .c source plus every header it transitively includes,
# so editing e.g. src/ph7/ph7int.h will rebuild every .o that pulls it in
# instead of leaving stale objects with the wrong struct layout.
-include $(MODE_OBJECTS:.o=.d)
