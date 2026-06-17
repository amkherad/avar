import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faArrowLeft, faCircleQuestion, faGear } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { ThemeToggle } from "./ThemeToggle";
import type { AppPage } from "./Sidebar";

export interface HeaderProps {
  page: AppPage;
  onNavigate: (page: AppPage) => void;
}

export function Header({ page, onNavigate }: HeaderProps) {
  const { t } = useTranslation();
  const isDashboard = page === "dashboard";

  return (
    <header className="avar-header">
      <div className="avar-header__brand">
        {!isDashboard ? (
          <Button
            variant="ghost"
            size="sm"
            className="avar-header__back"
            aria-label={t("nav.back")}
            onClick={() => onNavigate("dashboard")}
          >
            <FontAwesomeIcon icon={faArrowLeft} />
          </Button>
        ) : null}
        <img src="/icon.svg" alt="" className="avar-header__icon" />
        <h1 className="avar-header__title">
          {t("app.title")}
          <span className="avar-header__subtitle">{t("app.subtitle")}</span>
        </h1>
      </div>
      <div className="avar-header__actions">
        <ThemeToggle />
        <Button
          variant="ghost"
          size="sm"
          className={`avar-header__nav-btn ${page === "help" ? "avar-header__nav-btn--active" : ""}`}
          aria-label={t("nav.helpAria")}
          aria-current={page === "help" ? "page" : undefined}
          onClick={() => onNavigate(page === "help" ? "dashboard" : "help")}
        >
          <FontAwesomeIcon icon={faCircleQuestion} />
        </Button>
        <Button
          variant="ghost"
          size="sm"
          className={`avar-header__nav-btn ${page === "settings" ? "avar-header__nav-btn--active" : ""}`}
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
