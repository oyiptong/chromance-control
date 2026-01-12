import { useEffect, useState } from "preact/hooks";
import { apiGet, apiPost } from "../lib/api";
import { slugFromPathname } from "../lib/slug";

type ParamType = "int" | "float" | "bool" | "enum" | "color";
type ParamDescriptor = {
  id: number;
  name: string;
  displayName: string;
  type: ParamType;
  min: number;
  max: number;
  step: number;
  def: number;
  scale?: number;
};

type Stage = { id: number; name: string; displayName: string };

type EffectDetail = {
  id: number;
  canonicalSlug: string;
  aliasSlugs: string[];
  displayName: string;
  description?: string;
  supportsRestart: boolean;
  schema?: { params: ParamDescriptor[] };
  values?: Record<string, number | boolean>;
  stages?: { items: Stage[]; currentId: number };
};

export default function EffectDetailIsland() {
  const [slug, setSlug] = useState<string | null>(null);
  const [data, setData] = useState<EffectDetail | null>(null);
  const [values, setValues] = useState<Record<string, number | boolean>>({});
  const [err, setErr] = useState<string | null>(null);

  async function refresh(activeSlug: string) {
    setErr(null);
    const d = await apiGet<EffectDetail>("/api/effects/" + activeSlug);
    setData(d);
    setValues(d.values ?? {});
  }

  useEffect(() => {
    setSlug(typeof window === "undefined" ? null : slugFromPathname(window.location.pathname));
  }, []);

  useEffect(() => {
    if (!slug) return;
    refresh(slug).catch((e) => setErr(String(e)));
  }, [slug]);

  if (err) return <div class="alert alert-error">{err}</div>;
  if (!slug) return <div class="loading loading-spinner" />;
  if (!data) return <div class="loading loading-spinner" />;

  async function setParam(p: ParamDescriptor, next: number | boolean) {
    await apiPost("/api/effects/" + data.canonicalSlug + "/params", { items: [{ id: p.id, value: next }] });
    setValues((v) => ({ ...v, [p.name]: next }));
  }

  async function restart() {
    await apiPost("/api/effects/" + data.canonicalSlug + "/restart", {});
    await refresh(data.canonicalSlug);
  }

  function renderParamControl(p: ParamDescriptor) {
    const v = values[p.name] ?? p.def;

    if (p.type === "bool") {
      return (
        <input
          class="toggle toggle-primary"
          type="checkbox"
          checked={Boolean(v)}
          onChange={(e) => setParam(p, (e.target as HTMLInputElement).checked).catch((x) => setErr(String(x)))}
        />
      );
    }

    if (p.type === "enum") {
      const opts: number[] = [];
      for (let x = p.min; x <= p.max; x += p.step) opts.push(x);
      return (
        <select
          class="select select-bordered"
          value={Number(v)}
          onChange={(e) => setParam(p, parseInt((e.target as HTMLSelectElement).value, 10)).catch((x) => setErr(String(x)))}
        >
          {opts.map((x) => (
            <option key={x} value={x}>
              {x}
            </option>
          ))}
        </select>
      );
    }

    if (p.type === "color") {
      const cur = Number(v) >>> 0;
      const hex = "0x" + cur.toString(16).padStart(6, "0");
      return (
        <input
          class="input input-bordered font-mono"
          value={hex}
          onChange={(e) => {
            const t = (e.target as HTMLInputElement).value.trim();
            const n = t.startsWith("0x") ? parseInt(t.substring(2), 16) : parseInt(t, 16);
            if (!Number.isFinite(n)) return;
            setParam(p, n >>> 0).catch((x) => setErr(String(x)));
          }}
        />
      );
    }

    // int/float use range slider.
    const isFloat = p.type === "float";
    return (
      <div class="space-y-2">
        <div class="text-sm opacity-70 font-mono">{String(v)}</div>
        <input
          class="range range-primary"
          type="range"
          min={p.min}
          max={p.max}
          step={p.step}
          value={Number(v)}
          onInput={(e) => {
            const t = (e.target as HTMLInputElement).value;
            setValues((prev) => ({ ...prev, [p.name]: isFloat ? parseFloat(t) : parseInt(t, 10) }));
          }}
          onChange={(e) => {
            const t = (e.target as HTMLInputElement).value;
            setParam(p, isFloat ? parseFloat(t) : parseInt(t, 10)).catch((x) => setErr(String(x)));
          }}
        />
      </div>
    );
  }

  return (
    <div class="space-y-4">
      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="flex items-center justify-between gap-2">
            <div class="min-w-0">
              <div class="text-2xl font-bold truncate">{data.displayName}</div>
              <div class="text-sm opacity-70">{data.canonicalSlug}</div>
            </div>
            {data.supportsRestart ? (
              <button class="btn btn-sm" onClick={() => restart().catch((x) => setErr(String(x)))}>
                Restart
              </button>
            ) : null}
          </div>
          {data.description ? <div class="text-sm opacity-80 mt-2">{data.description}</div> : null}
        </div>
      </div>

      {data.stages?.items?.length ? (
        <div class="card bg-base-100 shadow">
          <div class="card-body">
            <div class="font-bold">Stage</div>
            <select
              class="select select-bordered mt-2"
              value={data.stages.currentId}
              onChange={(e) =>
                apiPost("/api/effects/" + data.canonicalSlug + "/stage", {
                  id: parseInt((e.target as HTMLSelectElement).value, 10)
                })
                  .then((d) => setData({ ...data, stages: { ...data.stages!, currentId: d.currentId } }))
                  .catch((x) => setErr(String(x)))
              }
            >
              {data.stages.items.map((s) => (
                <option key={s.id} value={s.id}>
                  {s.displayName}
                </option>
              ))}
            </select>
          </div>
        </div>
      ) : null}

      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Parameters</div>
          {!data.schema?.params?.length ? <div class="text-sm opacity-70">No parameters</div> : null}
          <div class="space-y-4">
            {(data.schema?.params ?? []).map((p) => (
              <div key={p.id} class="space-y-2">
                <div class="flex items-center justify-between gap-3">
                  <div class="min-w-0">
                    <div class="font-semibold">{p.displayName}</div>
                    <div class="text-xs opacity-70">{p.name}</div>
                  </div>
                  <button class="btn btn-xs" onClick={() => setParam(p, p.def).catch((x) => setErr(String(x)))}>
                    Reset
                  </button>
                </div>
                {renderParamControl(p)}
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
