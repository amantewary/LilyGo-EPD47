'use client';

import { TodoItem } from '@/lib/types';

interface TodoListProps {
  todos: TodoItem[];
}

export default function TodoList({ todos }: TodoListProps) {
  const displayTodos = todos.length > 0 ? todos.slice(0, 8) : [];

  return (
    <div className="h-full text-epd-black">
      <div className="mb-[22px] flex h-[24px] items-center gap-2">
        <span className="text-[18px] leading-none">□</span>
        <h2 className="text-[18px] font-semibold uppercase leading-none">TODO</h2>
      </div>
      <div className="flex flex-col gap-[14px]">
        {displayTodos.length === 0 ? (
          <div className="flex items-start gap-2 text-[18px] leading-snug">
            <span className="w-[22px] leading-none">□</span>
            <span>Nothing due today</span>
          </div>
        ) : (
          displayTodos.map((todo, index) => (
            <div key={index} className="flex items-start gap-2 text-[18px] leading-snug">
              <span className="w-[22px] leading-none">{todo.completed ? '☑' : '□'}</span>
              <span className={todo.completed ? 'line-through' : ''}>
                {todo.text.length > 42 ? `${todo.text.substring(0, 39)}...` : todo.text}
              </span>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
