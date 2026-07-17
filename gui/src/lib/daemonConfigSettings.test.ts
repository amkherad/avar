import { describe, expect, it, vi } from "vitest";
import type { DaemonClient } from "@/api/daemon";
import { DaemonApiError } from "@/api/daemon";
import { DOWNLOAD_CONFIG_DEFAULTS } from "@/lib/daemonConfigDefaults";
import { loadDaemonConfigValues, readDaemonConfigValue } from "@/lib/daemonConfigSettings";

describe("daemon config settings", () => {
  it("readDaemonConfigValue uses default when cli exec fails", async () => {
    const client = {
      cliExec: vi.fn(async () => {
        throw new DaemonApiError("RPC failed", 1);
      }),
    } as unknown as DaemonClient;

    const result = await readDaemonConfigValue(client, "dm.tempPath", "");

    expect(result.value).toBe("");
    expect(result.source).toBe("fallback");
  });

  it("loadDaemonConfigValues uses defaults when a key read fails", async () => {
    const client = {
      cliExec: vi.fn(async (argv: string[]) => {
        const key = argv[3];
        if (key === "dm.tempPath") {
          return { exitCode: 1 };
        }
        return { exitCode: 0, output: DOWNLOAD_CONFIG_DEFAULTS["dm.downloadPath"] };
      }),
    } as unknown as DaemonClient;

    const result = await loadDaemonConfigValues(
      client,
      ["dm.tempPath", "dm.downloadPath"],
      DOWNLOAD_CONFIG_DEFAULTS,
    );

    expect(result.values["dm.tempPath"]).toBe("");
    expect(result.values["dm.downloadPath"]).toBe("");
    expect(result.failedKeys).toEqual(["dm.tempPath"]);
  });

  it("loadDaemonConfigValues reads saved values from daemon", async () => {
    const client = {
      cliExec: vi.fn(async (argv: string[]) => {
        if (argv[3] === "dm.segmentation.concurrency") {
          return { exitCode: 0, output: "8\n" };
        }
        return { exitCode: 1 };
      }),
    } as unknown as DaemonClient;

    const result = await loadDaemonConfigValues(
      client,
      ["dm.segmentation.concurrency"],
      DOWNLOAD_CONFIG_DEFAULTS,
    );

    expect(result.values["dm.segmentation.concurrency"]).toBe("8");
    expect(result.failedKeys).toEqual([]);
  });
});
