export async function apiGet<T>(path: string): Promise<T> {
  const res = await fetch(path, { method: "GET" });
  const json = await res.json();
  if (!json.ok) throw new Error(json.error?.message ?? "API error");
  return json.data as T;
}

export async function apiPost<T>(path: string, body: unknown): Promise<T> {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });
  const json = await res.json();
  if (!json.ok) throw new Error(json.error?.message ?? "API error");
  return json.data as T;
}

export async function apiDelete<T>(path: string, body: unknown): Promise<T> {
  const res = await fetch(path, {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });
  const json = await res.json();
  if (!json.ok) throw new Error(json.error?.message ?? "API error");
  return json.data as T;
}

