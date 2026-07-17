import { useCallback, useEffect, useRef } from "react";
import type { DaemonClient } from "@/api/daemon";
import { shouldPersistDaemonConfigValue } from "@/lib/daemonConfigDefaults";
import { saveDaemonConfigValue } from "@/lib/daemonConfigSettings";

const PERSIST_DEBOUNCE_MS = 400;

export interface UseDaemonConfigPersistOptions {
  client: DaemonClient | null;
  ready: boolean;
  onSaveError: (key: string, err: unknown) => void;
  onSaveSuccess?: () => void;
}

export function useDaemonConfigPersist({
  client,
  ready,
  onSaveError,
  onSaveSuccess,
}: UseDaemonConfigPersistOptions) {
  const timersRef = useRef<Map<string, number>>(new Map());
  const pendingRef = useRef<Map<string, string>>(new Map());
  const clientRef = useRef(client);
  const readyRef = useRef(ready);
  const onSaveErrorRef = useRef(onSaveError);
  const onSaveSuccessRef = useRef(onSaveSuccess);

  clientRef.current = client;
  readyRef.current = ready;
  onSaveErrorRef.current = onSaveError;
  onSaveSuccessRef.current = onSaveSuccess;

  const clearTimer = useCallback((key: string) => {
    const timer = timersRef.current.get(key);
    if (timer !== undefined) {
      window.clearTimeout(timer);
      timersRef.current.delete(key);
    }
  }, []);

  const flushKey = useCallback(async (key: string) => {
    clearTimer(key);
    const value = pendingRef.current.get(key);
    if (value === undefined) {
      return;
    }
    pendingRef.current.delete(key);

    const activeClient = clientRef.current;
    if (!activeClient || !readyRef.current) {
      return;
    }
    if (!shouldPersistDaemonConfigValue(key, value)) {
      return;
    }

    try {
      await saveDaemonConfigValue(activeClient, key, value);
      onSaveSuccessRef.current?.();
    } catch (err) {
      onSaveErrorRef.current(key, err);
    }
  }, [clearTimer]);

  const flushAll = useCallback(async () => {
    const keys = [...pendingRef.current.keys()];
    await Promise.all(keys.map((key) => flushKey(key)));
  }, [flushKey]);

  const schedulePersist = useCallback(
    (key: string, value: string, immediate = false) => {
      if (!clientRef.current || !readyRef.current) {
        return;
      }
      if (!shouldPersistDaemonConfigValue(key, value)) {
        clearTimer(key);
        pendingRef.current.delete(key);
        return;
      }

      pendingRef.current.set(key, value);
      clearTimer(key);

      if (immediate) {
        void flushKey(key);
        return;
      }

      timersRef.current.set(
        key,
        window.setTimeout(() => {
          void flushKey(key);
        }, PERSIST_DEBOUNCE_MS),
      );
    },
    [clearTimer, flushKey],
  );

  useEffect(() => {
    return () => {
      for (const timer of timersRef.current.values()) {
        window.clearTimeout(timer);
      }
      timersRef.current.clear();

      const activeClient = clientRef.current;
      if (!activeClient || !readyRef.current) {
        pendingRef.current.clear();
        return;
      }

      const pending = [...pendingRef.current.entries()];
      pendingRef.current.clear();
      for (const [key, value] of pending) {
        if (shouldPersistDaemonConfigValue(key, value)) {
          void saveDaemonConfigValue(activeClient, key, value).catch((err) => {
            onSaveErrorRef.current(key, err);
          });
        }
      }
    };
  }, []);

  return { schedulePersist, flushKey, flushAll };
}
