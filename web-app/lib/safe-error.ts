import axios from 'axios';

export function toSafeError(error: unknown) {
  if (axios.isAxiosError(error)) {
    return {
      message: error.message,
      code: error.code,
      status: error.response?.status,
      statusText: error.response?.statusText,
    };
  }

  if (error instanceof Error) {
    return {
      message: error.message,
      name: error.name,
    };
  }

  return { message: String(error) };
}

export function logSafeError(context: string, error: unknown) {
  console.error(context, toSafeError(error));
}
