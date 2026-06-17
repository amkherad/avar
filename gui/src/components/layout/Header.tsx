import { useTranslation } from "react-i18next";
import { GearIcon } from "@/components/ui/GearIcon";
import { Button } from "@/components/ui/Button";
import type { AppPage } from "./Sidebar";

export interface HeaderProps {
  page: AppPage;
  onNavigate: (page: AppPage) => void;
}

export function Header({ page, onNavigate }: HeaderProps) {
  const { t } = useTranslation();

  return (
    <header className="avar-header">
      <div className="avar-header__brand">
        <h1>{t("app.title")}</h1>
        <p>{t("app.subtitle")}</p>
      </div>
      <Button
        variant="ghost"
        size="sm"
        className={`avar-header__settings ${page === "settings" ? "avar-header__settings--active" : ""}`}
        aria-label={t("nav.settingsAria")}
        aria-current={page === "settings" ? "page" : undefined}
        onClick={() => onNavigate(page === "settings" ? "dashboard" : "settings")}
      >
        <GearIcon />
      </Button>
    </header>
  );
}
