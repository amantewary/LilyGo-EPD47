'use client';

import { TodoItem } from '@/lib/types';

interface TodoListProps {
  todos: TodoItem[];
}

export default function TodoList({ todos }: TodoListProps) {
  if (todos.length === 0) {
    return (
      <div className="flex flex-col h-full">
        <div className="flex items-center justify-between mb-3">
          <h2 className="text-xs font-semibold tracking-[0.2em] text-epd-gray uppercase">Todo</h2>
          <div className="h-px flex-1 ml-3 bg-epd-gray/30" />
        </div>
        <div className="text-sm text-epd-gray">Nothing due today</div>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center justify-between mb-3">
        <h2 className="text-xs font-semibold tracking-[0.2em] text-epd-gray uppercase">Todo</h2>
        <div className="h-px flex-1 ml-3 bg-epd-gray/30" />
      </div>
      <div className="flex flex-col gap-3 overflow-y-auto pr-1">
        {todos.map((todo, index) => (
          <div key={index} className="flex flex-col gap-1 pb-2 border-b border-epd-gray/30 last:border-b-0 last:pb-0">
            <div className="flex items-start gap-2">
              <span className="text-lg mt-0.5">{todo.completed ? '☑' : '☐'}</span>
              <span
                className={`text-sm flex-1 leading-snug ${
                  todo.completed ? 'line-through text-epd-gray' : 'text-epd-black'
                }`}
              >
                {todo.text}
              </span>
            </div>
            {todo.due && (
              <span className="text-[11px] text-epd-gray pl-6 uppercase tracking-[0.12em]">
                Due {todo.due}
              </span>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
