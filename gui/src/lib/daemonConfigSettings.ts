import type { DaemonClient } from "@/api/daemon";
import { appLogger } from "@/lib/appLogger";
import { configDefaultValue } from "@/lib/daemonConfigDefaults";
import type { TFunction } from "i18next";

export type ConfigReadSource = "daemon" | "fallback";

export interface ConfigReadResult {
  value: string | null;
  source: ConfigReadSource;
}

export interface DaemonConfigLoadResult {
  values: Record<string, string>;
  failedKeys: string[];
}

export async function readDaemonConfigValue(
  client: DaemonClient,
  key: string,
  defaultValue: string,
): Promise<ConfigReadResult> {
  const argv = ["avar", "config", "get", key];
  if (defaultValue !== "") {
    argv.push(`--defaultValue=${defaultValue}`);
  }

  try {
    const result = await client.cliExec(argv);
    if (result.exitCode !== 0) {
      return { value: defaultValue, source: "fallback" };
    }
    if (result.output === undefined || result.output === null) {
      return { value: defaultValue, source: "fallback" };
    }
    return { value: result.output.trimEnd(), source: "daemon" };
  } catch (err) {
    appLogger.gui.warn("Daemon config read failed; using default", key, String(err));
    return { value: defaultValue, source: "fallback" };
  }
}

export async function loadDaemonConfigValues(
  client: DaemonClient,
  keys: readonly string[],
  defaults: Record<string, string>,
): Promise<DaemonConfigLoadResult> {
  const values: Record<string, string> = {};
  const failedKeys: string[] = [];

  for (const key of keys) {
    const defaultValue = configDefaultValue(defaults, key);
    const result = await readDaemonConfigValue(client, key, defaultValue);
    values[key] = result.value ?? defaultValue;
    if (result.source === "fallback") {
      failedKeys.push(key);
    }
  }

  return { values, failedKeys };
}

export async function saveDaemonConfigValue(
  client: DaemonClient,
  key: string,
  value: string,
): Promise<void> {
  await client.setConfig(key, value);
}

export function daemonConfigLoadWarning(
  t: TFunction,
  failedKeys: string[],
): string | null {
  if (failedKeys.length === 0) {
    return null;
  }
  return t("settings.configLoadWarning", { count: failedKeys.length });
}

export function daemonConfigSaveError(
  t: TFunction,
  key: string,
  err: unknown,
): string {
  const message = err instanceof Error ? err.message : t("common.error");
  return t("settings.configSaveFailed", { key, message });
}
