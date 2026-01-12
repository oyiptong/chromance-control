import { useEffect, useState } from "preact/hooks";
import { apiDelete, apiGet } from "../lib/api";

type Summary = {
  globals: { aeid: number; bright_pct: number; mode: number };
  effects: { id: number; canonicalSlug: string; present: boolean }[];
  confirmToken: string;
};

type EffectBlob = {
  id: number;
  canonicalSlug: string;
  present: boolean;
  blobHex?: string;
  blobVersion?: number;
  crc16?: number;
};

export default function PersistenceIsland() {
  const [summary, setSummary] = useState<Summary | null>(null);
  const [selected, setSelected] = useState<string>("");
  const [detail, setDetail] = useState<EffectBlob | null>(null);
  const [err, setErr] = useState<string | null>(null);

  useEffect(() => {
    apiGet<Summary>("/api/settings/persistence/summary")
      .then((d) => {
        setSummary(d);
        if (d.effects.length) setSelected(d.effects[0].canonicalSlug);
      })
      .catch((e) => setErr(String(e)));
  }, []);

  useEffect(() => {
    if (!selected) return;
    apiGet<EffectBlob>("/api/settings/persistence/effects/" + selected).then(setDetail).catch((e) => setErr(String(e)));
  }, [selected]);

  if (err) return <div class="alert alert-error">{err}</div>;
  if (!summary) return <div class="loading loading-spinner" />;

  return (
    <div class="space-y-4">
      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Summary</div>
          <pre class="mt-2 p-3 bg-base-200 rounded overflow-auto text-xs">{JSON.stringify(summary, null, 2)}</pre>
        </div>
      </div>

      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Effect Blob</div>
          <select
            class="select select-bordered w-full mt-2"
            value={selected}
            onChange={(e) => setSelected((e.target as HTMLSelectElement).value)}
          >
            {summary.effects.map((e) => (
              <option key={e.canonicalSlug} value={e.canonicalSlug}>
                {e.canonicalSlug} {e.present ? "" : "(missing)"}
              </option>
            ))}
          </select>
          {detail ? (
            <pre class="mt-3 p-3 bg-base-200 rounded overflow-auto text-xs">{JSON.stringify(detail, null, 2)}</pre>
          ) : (
            <div class="mt-3 text-sm opacity-70">Loading…</div>
          )}
        </div>
      </div>

      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Wipe All</div>
          <div class="text-sm opacity-70">Requires confirmToken + phrase DELETE.</div>
          <button
            class="btn btn-error btn-sm mt-2"
            onClick={() =>
              apiDelete("/api/settings/persistence", { confirmToken: summary.confirmToken, confirmPhrase: "DELETE" })
                .then(() => apiGet<Summary>("/api/settings/persistence/summary").then(setSummary))
                .catch((x) => setErr(String(x)))
            }
          >
            Delete All Persisted Settings
          </button>
        </div>
      </div>
    </div>
  );
}

