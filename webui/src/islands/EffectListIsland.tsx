import { useEffect, useState } from "preact/hooks";
import { apiGet, apiPost } from "../lib/api";

type Effect = { id: number; canonicalSlug: string; displayName: string; description?: string };
type EffectsListResponse = { effects: Effect[]; activeId: number; activeCanonicalSlug: string };

export default function EffectListIsland() {
  const [data, setData] = useState<EffectsListResponse | null>(null);
  const [err, setErr] = useState<string | null>(null);

  async function refresh() {
    setErr(null);
    const d = await apiGet<EffectsListResponse>("/api/effects");
    setData(d);
  }

  useEffect(() => {
    refresh().catch((e) => setErr(String(e)));
  }, []);

  if (err) return <div class="alert alert-error">{err}</div>;
  if (!data) return <div class="loading loading-spinner" />;

  return (
    <div class="space-y-3">
      {data.effects.map((e) => (
        <div class="card bg-base-100 shadow" key={e.canonicalSlug}>
          <div class="card-body">
            <div class="flex items-center justify-between gap-2">
              <div class="min-w-0">
                <div class="font-bold truncate">{e.displayName}</div>
                <div class="text-sm opacity-70">{e.canonicalSlug}</div>
              </div>
              <div class="flex gap-2">
                <a class="btn btn-sm" href={`/effects/${e.canonicalSlug}`}>
                  Open
                </a>
                <button
                  class="btn btn-sm btn-primary"
                  disabled={data.activeCanonicalSlug === e.canonicalSlug}
                  onClick={() =>
                    apiPost("/api/effects/" + e.canonicalSlug + "/activate", {})
                      .then(() => refresh())
                      .catch((x) => setErr(String(x)))
                  }
                >
                  Activate
                </button>
              </div>
            </div>
            {e.description ? <div class="text-sm opacity-80 mt-2">{e.description}</div> : null}
          </div>
        </div>
      ))}
    </div>
  );
}

