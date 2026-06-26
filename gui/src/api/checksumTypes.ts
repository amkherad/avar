export const CHECKSUM_ALGORITHMS = [
  "crc32",
  "md5",
  "sha1",
  "sha224",
  "sha256",
  "sha384",
  "sha512",
  "ripemd160",
] as const;

export type ChecksumAlgorithm = (typeof CHECKSUM_ALGORITHMS)[number];

export interface DownloadChecksumResult {
  exitCode: number;
  checksum?: string;
  match?: boolean;
}
