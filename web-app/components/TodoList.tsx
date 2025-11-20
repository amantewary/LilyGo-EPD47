'use client';

import { TodoItem } from '@/lib/types';

interface TodoListProps {
  todos: TodoItem[];
}

export default function TodoList({ todos }: TodoListProps) {
  if (todos.length === 0) {
    return (
      <div className="flex flex-col h-full">
        <h2 className="text-lg font-semibold mb-4 text-epd-black">TODO</h2>
        <div className="text-sm text-epd-gray">Nothing due today</div>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      <h2 className="text-lg font-semibold mb-4 text-epd-black">TODO</h2>
      <div className="flex flex-col gap-2 overflow-y-auto">
        {todos.map((todo, index) => (
          <div key={index} className="flex items-start gap-2">
            <span className="text-lg mt-0.5">
              {todo.completed ? '☑' : '☐'}
            </span>
            <span className={`text-sm flex-1 ${todo.completed ? 'line-through text-epd-gray' : 'text-epd-black'}`}>
              {todo.text}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

