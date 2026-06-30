import { useEffect, useRef } from "react";
import { useTranslation } from "react-i18next";
import { useConfigStore } from "@/stores/configStore";

export function useElectronTrayLabels(): void {
  const { t } = useTranslation();
  const locale = useConfigStore((s) => s.config.locale);
  const lastPayloadRef = useRef<string | null>(null);

  useEffect(() => {
    if (!window.avar?.isElectron) {
      return;
    }

    const labels = {
      show: t("tray.show"),
      exit: t("tray.exit"),
      startAll: t("tray.startAll"),
      pauseAll: t("tray.pauseAll"),
      resumeAll: t("tray.resumeAll"),
      stopAll: t("tray.stopAll"),
    };

    const payloadKey = JSON.stringify(labels);
    if (payloadKey === lastPayloadRef.current) {
      return;
    }

    lastPayloadRef.current = payloadKey;
    void window.avar.setTrayLabels(labels);
  }, [t, locale]);
}
