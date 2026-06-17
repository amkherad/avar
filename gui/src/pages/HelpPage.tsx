import { useState } from "react";
import { useTranslation } from "react-i18next";
import Markdown from "react-markdown";
import { useConfigStore } from "@/stores/configStore";
import { HELP_TOPICS, getHelpContent } from "@/lib/helpDocs";

export function HelpPage() {
  const { t } = useTranslation();
  const locale = useConfigStore((s) => s.config.locale);
  const [topicId, setTopicId] = useState(HELP_TOPICS[0].id);

  const topic = HELP_TOPICS.find((item) => item.id === topicId) ?? HELP_TOPICS[0];
  const content = getHelpContent(locale, topic.file);

  return (
    <div className="avar-help-page">
      <aside className="avar-help-page__sidebar">
        <h2 className="avar-help-page__sidebar-title">{t("help.title")}</h2>
        <nav className="avar-help-page__nav">
          {HELP_TOPICS.map((item) => (
            <button
              key={item.id}
              type="button"
              className={`avar-help-page__nav-item ${topicId === item.id ? "avar-help-page__nav-item--active" : ""}`}
              onClick={() => setTopicId(item.id)}
            >
              {t(item.titleKey)}
            </button>
          ))}
        </nav>
      </aside>

      <article className="avar-help-page__content avar-markdown">
        {content ? <Markdown>{content}</Markdown> : <p>{t("help.notFound")}</p>}
      </article>
    </div>
  );
}
