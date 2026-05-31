#pragma once

#include "types.h"
#include <vector>

// Fetch today's and overdue todos from Home Assistant into todoList
void fetchTodos(std::vector<TodoItem> &todoList);
