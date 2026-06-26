import { proxySettingsToRpcParams, type ProxySettings } from "@/lib/proxySettings";
import { parseDownloadItem, parseQueueRecord, parseSnapshotPayload } from "./snapshot";
import type { DownloadChecksumResult } from "./checksumTypes";
import type {
  CliExecResult,
  DirectoryBrowseResult,
  DownloadInfo,
  HealthInfo,
  JsonRpcResponse,
  QueueAddParams,
  QueueInfo,
  QueueRpcResult,
  SnapshotPayload,
  SystemStatsInfo,
} from "./types";

export interface AddDownloadParams {
  url: string;
  queue?: string;
  name?: string;
  attached?: boolean;
  startNow?: boolean;
  outputPath?: string;
  group?: string;
  proxy?: ProxySettings;
  referer?: string;
  streamKind?: string;
  forceNew?: boolean;
}

let requestId = 1;

function nextId(): number {
  requestId += 1;
  return requestId;
}

export class DaemonApiError extends Error {
  readonly code: number;

  constructor(message: string, code = -1) {
    super(message);
    this.name = "DaemonApiError";
    this.code = code;
  }
}

export interface DaemonClientOptions {
  baseUrl: string;
  authToken?: string;
  useRelativeApi?: boolean;
}

export class DaemonClient {
  private readonly rpcUrl: string;
  private readonly healthUrl: string;
  private readonly statsUrl: string;
  private readonly eventsUrl: string;
  private readonly wsUrl: string;
  private readonly authToken?: string;
  private readonly baseUrlTrimmed: string;
  private readonly useRelativeApi: boolean;

  constructor(options: DaemonClientOptions) {
    const trimmed = options.baseUrl.replace(/\/+$/, "");
    const useRelative = options.useRelativeApi ?? false;
    this.baseUrlTrimmed = trimmed;
    this.useRelativeApi = useRelative;

    if (useRelative) {
      this.rpcUrl = "/api/rpc";
      this.healthUrl = "/api/health";
      this.statsUrl = "/api/stats";
      this.eventsUrl = "/api/events";
      this.wsUrl = `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}/api/ws`;
    } else {
      this.rpcUrl = `${trimmed}/api/rpc`;
      this.healthUrl = `${trimmed}/api/health`;
      this.statsUrl = `${trimmed}/api/stats`;
      this.eventsUrl = `${trimmed}/api/events`;
      const wsBase = trimmed.replace(/^http/i, "ws");
      this.wsUrl = `${wsBase}/api/ws`;
    }

    this.authToken = options.authToken?.trim() || undefined;
  }

  private headers(): HeadersInit {
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
    };
    if (this.authToken) {
      headers.Authorization = `Bearer ${this.authToken}`;
    }
    return headers;
  }

  getEventsUrl(): string {
    return this.eventsUrl;
  }

  getWebSocketUrl(): string {
    return this.wsUrl;
  }

  async ping(signal?: AbortSignal): Promise<boolean> {
    try {
      const stats = await this.systemStats(signal);
      return stats.status === "ok";
    } catch {
      return false;
    }
  }

  async health(signal?: AbortSignal): Promise<HealthInfo> {
    const res = await fetch(this.healthUrl, {
      headers: this.headers(),
      signal,
    });
    if (!res.ok) {
      throw new DaemonApiError(`Health check failed (${res.status})`, res.status);
    }
    return (await res.json()) as HealthInfo;
  }

  async rpc<T>(
    method: string,
    params: Record<string, unknown> = {},
    signal?: AbortSignal,
  ): Promise<T> {
    const payload = {
      jsonrpc: "2.0",
      method,
      params,
      id: nextId(),
    };

    const res = await fetch(this.rpcUrl, {
      method: "POST",
      headers: this.headers(),
      body: JSON.stringify(payload),
      signal,
    });

    if (!res.ok) {
      throw new DaemonApiError(`RPC request failed (${res.status})`, res.status);
    }

    const body = (await res.json()) as JsonRpcResponse<T>;
    if (body.error) {
      throw new DaemonApiError(body.error.message, body.error.code);
    }
    if (body.result === undefined) {
      throw new DaemonApiError("RPC response missing result");
    }
    return body.result;
  }

  async cliExec(argv: string[]): Promise<CliExecResult> {
    return this.rpc<CliExecResult>("cli.exec", { argv });
  }

  async listQueues(): Promise<QueueInfo[]> {
    const result = await this.rpc<{ exitCode: number; queues?: unknown[] }>("queue.list");
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to list queues", result.exitCode);
    }
    return (result.queues ?? []).map((item) => parseQueueRecord(item));
  }

  async listDownloads(): Promise<DownloadInfo[]> {
    const result = await this.rpc<{ exitCode: number; downloads?: unknown[] }>(
      "downloads.list",
    );
    if (result.exitCode !== 0) {
      return [];
    }
    return (result.downloads ?? []).map((item) => parseDownloadItem(item));
  }

  async watchDownloadProgress(id: string): Promise<void> {
    await this.rpc<{ exitCode: number }>("download.watch", { id });
  }

  async unwatchDownloadProgress(id: string): Promise<void> {
    await this.rpc<{ exitCode: number }>("download.unwatch", { id });
  }

  async addQueue(params: QueueAddParams): Promise<string> {
    const result = await this.rpc<QueueRpcResult>("queue.add", { ...params });
    if (result.exitCode !== 0 || !result.id) {
      throw new DaemonApiError("Failed to add queue", result.exitCode ?? -1);
    }
    return result.id;
  }

  async removeQueue(id: string, purgeItems = false): Promise<void> {
    const result = await this.rpc<QueueRpcResult>("queue.remove", {
      id,
      purgeItems,
    });
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to remove queue", result.exitCode ?? -1);
    }
  }

  async editQueue(
    id: string,
    patch: Partial<Pick<QueueAddParams, "description">>,
  ): Promise<void> {
    const result = await this.rpc<QueueRpcResult>("queue.edit", { id, ...patch });
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to edit queue", result.exitCode ?? -1);
    }
  }

  async startQueue(id: string): Promise<void> {
    const result = await this.rpc<QueueRpcResult>("queue.start", { id });
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to start queue", result.exitCode ?? -1);
    }
  }

  async stopQueue(id: string): Promise<void> {
    const result = await this.rpc<QueueRpcResult>("queue.stop", { id });
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to stop queue", result.exitCode ?? -1);
    }
  }

  async getLogs(maxLines = 100, since = 0): Promise<{ logs: string; nextOffset: number }> {
    const result = await this.rpc<{ logs?: string; nextOffset?: number }>("logs.get", {
      maxLines,
      since,
    });
    return {
      logs: result.logs ?? "",
      nextOffset: result.nextOffset ?? since,
    };
  }

  async addDownload(options: AddDownloadParams | string, queueName?: string): Promise<void> {
    const params: Record<string, unknown> =
      typeof options === "string"
        ? { url: options, attached: false, ...(queueName ? { queue: queueName } : {}) }
        : {
            url: options.url,
            attached: options.attached ?? false,
            ...(options.startNow ? { startNow: true } : {}),
            ...(options.queue ? { queue: options.queue } : {}),
            ...(options.name ? { name: options.name } : {}),
            ...(options.outputPath ? { outputPath: options.outputPath } : {}),
            ...(options.group ? { group: options.group } : {}),
            ...(options.proxy ? { proxy: proxySettingsToRpcParams(options.proxy) } : {}),
            ...(options.referer ? { referer: options.referer } : {}),
            ...(options.streamKind ? { streamKind: options.streamKind } : {}),
            ...(options.forceNew ? { forceNew: true } : {}),
          };

    const controller = new AbortController();
    const timer = window.setTimeout(() => controller.abort(), 30_000);
    try {
      const result = await this.rpc<{ exitCode: number }>(
        "download.add",
        params,
        controller.signal,
      );
      if (result.exitCode !== 0) {
        throw new DaemonApiError("Failed to add download", result.exitCode);
      }
    } catch (err) {
      if (err instanceof DOMException && err.name === "AbortError") {
        throw new DaemonApiError("Timed out waiting for daemon to queue download", -1);
      }
      throw err;
    } finally {
      window.clearTimeout(timer);
    }
  }

  async pauseDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "pause", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to pause download", result.exitCode);
    }
  }

  async resumeDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "resume", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to resume download", result.exitCode);
    }
  }

  async startDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "start", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to start download", result.exitCode);
    }
  }

  async stopDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "stop", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to stop download", result.exitCode);
    }
  }

  async restartDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "restart", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to restart download", result.exitCode);
    }
  }

  async dismissResumePrompt(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "dismiss-resume", id]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to dismiss resume prompt", result.exitCode);
    }
  }

  async computeDownloadChecksum(
    id: string,
    algorithm: string,
    expected?: string,
    signal?: AbortSignal,
  ): Promise<DownloadChecksumResult> {
    return this.rpc<DownloadChecksumResult>(
      "download.checksum",
      {
        id,
        algorithm,
        ...(expected ? { expected } : {}),
      },
      signal,
    );
  }

  async removeDownload(id: string, purgeFiles = false): Promise<void> {
    const argv = ["avar", "dl", "rm", id, "--force"];
    if (purgeFiles) {
      argv.push("--purge-files");
    }
    const result = await this.cliExec(argv);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to remove download", result.exitCode);
    }
  }

  getDownloadFileUrl(id: string): string {
    return `${this.fileBaseUrl()}/api/downloads/${encodeURIComponent(id)}/file`;
  }

  private fileBaseUrl(): string {
    if (this.useRelativeApi) {
      return "";
    }
    return this.baseUrlTrimmed;
  }

  async getConfig(key: string, defaultValue?: string): Promise<string | null> {
    const argv = ["avar", "config", "get", key];
    if (defaultValue !== undefined) {
      argv.push(`--defaultValue=${defaultValue}`);
    }
    const result = await this.cliExec(argv);
    if (result.exitCode !== 0) {
      return defaultValue ?? null;
    }
    const output = result.output?.trim();
    return output && output.length > 0 ? output : (defaultValue ?? null);
  }

  async setConfig(key: string, value: string): Promise<void> {
    const result = await this.cliExec(["avar", "config", "set", key, value]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError(`Failed to set config ${key}`, result.exitCode);
    }
  }

  async systemStats(signal?: AbortSignal): Promise<SystemStatsInfo> {
    const res = await fetch(this.statsUrl, {
      headers: this.headers(),
      signal,
    });
    if (!res.ok) {
      throw new DaemonApiError(`Stats request failed (${res.status})`, res.status);
    }
    return (await res.json()) as SystemStatsInfo;
  }

  async browseDirectory(path = "", signal?: AbortSignal): Promise<DirectoryBrowseResult> {
    return this.rpc<DirectoryBrowseResult>("fs.browse", { path }, signal);
  }

  parseSnapshot(raw: unknown): SnapshotPayload | null {
    return parseSnapshotPayload(raw);
  }
}

export function createDaemonClient(session: {
  baseUrl: string;
  authToken?: string;
  useRelativeApi?: boolean;
}): DaemonClient {
  return new DaemonClient(session);
}

export function buildDownloadFileUrl(
  session: { baseUrl: string; useRelativeApi?: boolean },
  downloadId: string,
): string {
  const client = createDaemonClient(session);
  return client.getDownloadFileUrl(downloadId);
}
