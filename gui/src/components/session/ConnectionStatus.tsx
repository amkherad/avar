import { useTranslation } from "react-i18next";
import type { ConnectionState } from "@/stores/connectionStore";

export interface ConnectionStatusProps {
  state: ConnectionState;
}

export function ConnectionStatus({ state }: ConnectionStatusProps) {
  const { t } = useTranslation();

  const label =
    state === "connected"
      ? t("session.connected")
      : state === "connecting"
        ? t("session.connecting")
        : t("session.disconnected");

  return (
    <div className="avar-connection" aria-live="polite">
      <span
        className={`avar-connection__dot ${state === "connected" ? "avar-connection__dot--ok" : ""}`}
      />
      <span>{label}</span>
    </div>
  );
}
