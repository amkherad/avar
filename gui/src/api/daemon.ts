import { parseDownloadItem, parseQueueRecord, parseSnapshotPayload } from "./snapshot";
import type {
  CliExecResult,
  DownloadInfo,
  HealthInfo,
  JsonRpcResponse,
  QueueAddParams,
  QueueInfo,
  QueueRpcResult,
  SnapshotPayload,
} from "./types";

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
  private readonly pingUrl: string;
  private readonly eventsUrl: string;
  private readonly wsUrl: string;
  private readonly authToken?: string;

  constructor(options: DaemonClientOptions) {
    const trimmed = options.baseUrl.replace(/\/+$/, "");
    const useRelative = options.useRelativeApi ?? false;

    if (useRelative) {
      this.rpcUrl = "/api/rpc";
      this.healthUrl = "/api/health";
      this.pingUrl = "/api/ping";
      this.eventsUrl = "/api/events";
      this.wsUrl = `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}/api/ws`;
    } else {
      this.rpcUrl = `${trimmed}/api/rpc`;
      this.healthUrl = `${trimmed}/api/health`;
      this.pingUrl = `${trimmed}/api/ping`;
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
      const res = await fetch(this.pingUrl, {
        headers: this.headers(),
        signal,
      });
      if (!res.ok) {
        return false;
      }
      const body = (await res.json()) as { status?: string };
      return body.status === "ok";
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

  async addQueue(params: QueueAddParams): Promise<string> {
    const result = await this.rpc<QueueRpcResult>("queue.add", params as Record<string, unknown>);
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

  async getLogs(maxLines = 100): Promise<string> {
    const result = await this.rpc<{ logs?: string }>("logs.get", { maxLines });
    return result.logs ?? "";
  }

  async addDownload(url: string, queueName?: string): Promise<void> {
    const params: Record<string, unknown> = { url, attached: false };
    if (queueName) {
      params.queue = queueName;
    }
    const result = await this.rpc<{ exitCode: number }>("download.add", params);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to add download", result.exitCode);
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

  async removeDownload(id: string): Promise<void> {
    const result = await this.cliExec(["avar", "dl", "rm", id, "--force"]);
    if (result.exitCode !== 0) {
      throw new DaemonApiError("Failed to remove download", result.exitCode);
    }
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
