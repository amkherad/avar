import { useTranslation } from "react-i18next";
import { useConfigStore } from "@/stores/configStore";
import { HELP_TOPICS, getHelpContent } from "@/lib/helpDocs";
import { MarkdownRenderer } from "@/components/ui/MarkdownRenderer";

export interface HelpPageProps {
  topicId: string;
}

export function HelpPage({ topicId }: HelpPageProps) {
  const { t } = useTranslation();
  const locale = useConfigStore((s) => s.config.locale);

  const topic = HELP_TOPICS.find((item) => item.id === topicId) ?? HELP_TOPICS[0];
  const content = getHelpContent(locale, topic.file);

  return (
    <article className="avar-help-page__content avar-markdown">
      {content ? (
        <MarkdownRenderer content={content} />
      ) : (
        <p>{t("help.notFound")}</p>
      )}
    </article>
  );
}
