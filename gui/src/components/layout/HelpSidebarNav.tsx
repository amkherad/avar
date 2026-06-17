import { useTranslation } from "react-i18next";
import { HELP_TOPICS } from "@/lib/helpDocs";

export interface HelpSidebarNavProps {
  topicId: string;
  onTopicChange: (id: string) => void;
}

export function HelpSidebarNav({ topicId, onTopicChange }: HelpSidebarNavProps) {
  const { t } = useTranslation();

  return (
    <>
      <h2 className="avar-sidebar-nav__title">{t("help.title")}</h2>
      <nav className="avar-sidebar-nav">
        {HELP_TOPICS.map((item) => (
          <button
            key={item.id}
            type="button"
            className={`avar-sidebar-nav__item ${topicId === item.id ? "avar-sidebar-nav__item--active" : ""}`}
            onClick={() => onTopicChange(item.id)}
          >
            {t(item.titleKey)}
          </button>
        ))}
      </nav>
    </>
  );
}
