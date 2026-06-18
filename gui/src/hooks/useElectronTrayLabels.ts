import { useEffect } from "react";
import { useTranslation } from "react-i18next";
import { useConfigStore } from "@/stores/configStore";

export function useElectronTrayLabels(): void {
  const { t } = useTranslation();
  const locale = useConfigStore((s) => s.config.locale);

  useEffect(() => {
    if (!window.avar?.isElectron) {
      return;
    }
    void window.avar.setTrayLabels({
      show: t("tray.show"),
      exit: t("tray.exit"),
      startAll: t("tray.startAll"),
      pauseAll: t("tray.pauseAll"),
      resumeAll: t("tray.resumeAll"),
      stopAll: t("tray.stopAll"),
    });
  }, [t, locale]);
}
