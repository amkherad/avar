export type DownloadStatus =

  | "queued"

  | "downloading"

  | "paused"

  | "completed"

  | "error"

  | "cancelled"

  | string;



export interface QueueInfo {

  id: string;

  name: string;

  description?: string;

  running: boolean;

  /** Synthetic queues (e.g. default) cannot be edited or removed. */
  readonly?: boolean;

}



export interface QueueAddParams {

  name: string;

  description?: string;

}



export interface QueueRpcResult {

  exitCode: number;

  id?: string;

}



export interface DownloadInfo {

  id: string;

  filename: string;

  url?: string;

  status: DownloadStatus;

  queueId?: string;

  bytesDownloaded: number;

  totalBytes: number;

}



export interface HealthInfo {

  status: string;

  queueCount: number;

  activeDownloads: number;

  uptimeSeconds: number;

  downloads: Array<{

    id: string;

    filename: string;

    bytesDownloaded: number;

    totalBytes: number;

  }>;

}



export interface SnapshotPayload {

  type?: string;

  health?: HealthInfo;

  queues?: QueueInfo[];

  downloads?: DownloadInfo[];

}



export interface JsonRpcResponse<T> {

  jsonrpc: string;

  id: number | string | null;

  result?: T;

  error?: { code: number; message: string };

}



export interface CliExecResult {

  exitCode: number;

  output?: string;

}



export interface DaemonSession {

  id: string;

  label: string;

  baseUrl: string;

  authToken?: string;

  useRelativeApi?: boolean;

}


