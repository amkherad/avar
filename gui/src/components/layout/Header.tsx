import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faGear } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { ThemeToggle } from "./ThemeToggle";
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
        <img src="/icon.svg" alt="" className="avar-header__icon" width={36} height={36} />
        <div>
          <h1>{t("app.title")}</h1>
          <p>{t("app.subtitle")}</p>
        </div>
      </div>
      <div className="avar-header__actions">
        <ThemeToggle />
        <Button
          variant="ghost"
          size="sm"
          className={`avar-header__settings ${page === "settings" ? "avar-header__settings--active" : ""}`}
          aria-label={t("nav.settingsAria")}
          aria-current={page === "settings" ? "page" : undefined}
          onClick={() => onNavigate(page === "settings" ? "dashboard" : "settings")}
        >
          <FontAwesomeIcon icon={faGear} />
        </Button>
      </div>
    </header>
  );
}
