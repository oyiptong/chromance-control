import { useEffect, useState } from "preact/hooks";
import { apiGet, apiPost } from "../lib/api";

type Settings = {
  firmwareVersion: string;
  mappingVersion?: string;
  activeId: number;
  activeCanonicalSlug: string;
  brightness: { softPct: number; hwCeilingPct: number; effectivePct: number };
};
type PersistenceSummary = { confirmToken: string };

export default function SettingsIsland() {
  const [data, setData] = useState<Settings | null>(null);
  const [err, setErr] = useState<string | null>(null);
  const [softPct, setSoftPct] = useState<number>(0);
  const [confirmToken, setConfirmToken] = useState<string>("");
  const [confirmPhrase, setConfirmPhrase] = useState<string>("");

  useEffect(() => {
    apiGet<Settings>("/api/settings")
      .then((d) => {
        setData(d);
        setSoftPct(d.brightness.softPct);
      })
      .catch((e) => setErr(String(e)));

    apiGet<PersistenceSummary>("/api/settings/persistence/summary")
      .then((d) => setConfirmToken(d.confirmToken))
      .catch(() => setConfirmToken(""));
  }, []);

  if (err) return <div class="alert alert-error">{err}</div>;
  if (!data) return <div class="loading loading-spinner" />;

  return (
    <div class="space-y-4">
      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Info</div>
          <div class="text-sm opacity-70">Firmware: {data.firmwareVersion}</div>
          {data.mappingVersion ? <div class="text-sm opacity-70">Mapping: {data.mappingVersion}</div> : null}
          <div class="text-sm opacity-70">Active: {data.activeCanonicalSlug}</div>
        </div>
      </div>

      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Brightness</div>
          <div class="text-sm opacity-70">
            soft={data.brightness.softPct}% ceiling={data.brightness.hwCeilingPct}% effective={data.brightness.effectivePct}%
          </div>
          <input
            class="range range-primary"
            type="range"
            min={0}
            max={100}
            step={10}
            value={softPct}
            onInput={(e) => setSoftPct(parseInt((e.target as HTMLInputElement).value, 10))}
            onChange={() =>
              apiPost("/api/settings/brightness", { softPct })
                .then((d) => setData({ ...data, brightness: d }))
                .catch((x) => setErr(String(x)))
            }
          />
        </div>
      </div>

      <div class="card bg-base-100 shadow">
        <div class="card-body">
          <div class="font-bold">Reset</div>
          <div class="text-sm opacity-70">Type RESET to confirm.</div>
          <input
            class="input input-bordered mt-2"
            placeholder="RESET"
            value={confirmPhrase}
            onInput={(e) => setConfirmPhrase((e.target as HTMLInputElement).value)}
          />
          <button
            class="btn btn-error btn-sm mt-2"
            disabled={!confirmToken || confirmPhrase !== "RESET"}
            onClick={() =>
              apiPost("/api/settings/reset", { confirmToken, confirmPhrase: "RESET" }).catch((x) => setErr(String(x)))
            }
          >
            Reboot Device
          </button>
        </div>
      </div>
    </div>
  );
}

