import { useEffect, useId, useRef } from "react";
import Markdown from "react-markdown";
import remarkGfm from "remark-gfm";
import mermaid from "mermaid";
import type { Components } from "react-markdown";
import type { PluggableList } from "unified";

export interface MarkdownRendererProps {
  content: string;
  /** Additional remark/rehype plugins appended after built-in GFM support. */
  remarkPlugins?: PluggableList;
  className?: string;
}

let mermaidInitialized = false;

function ensureMermaid() {
  if (mermaidInitialized) {
    return;
  }
  mermaid.initialize({
    startOnLoad: false,
    theme: "neutral",
    securityLevel: "strict",
  });
  mermaidInitialized = true;
}

function MermaidBlock({ code }: { code: string }) {
  const id = useId().replace(/:/g, "");
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    ensureMermaid();
    const container = containerRef.current;
    if (!container) {
      return;
    }

    let cancelled = false;

    async function render() {
      try {
        const { svg } = await mermaid.render(`mermaid-${id}`, code);
        if (!cancelled) {
          container.innerHTML = svg;
        }
      } catch {
        if (!cancelled) {
          container.textContent = code;
        }
      }
    }

    void render();
    return () => {
      cancelled = true;
    };
  }, [code, id]);

  return <div ref={containerRef} className="avar-markdown__mermaid" />;
}

const defaultComponents: Components = {
  table({ children }) {
    return (
      <div className="avar-markdown__table-wrap">
        <table>{children}</table>
      </div>
    );
  },
  code({ className, children, ...props }) {
    const text = String(children).replace(/\n$/, "");
    const match = /language-(\w+)/.exec(className ?? "");
    const language = match?.[1];

    if (language === "mermaid") {
      return <MermaidBlock code={text} />;
    }

    const inline = !className;
    if (inline) {
      return <code {...props}>{children}</code>;
    }

    return (
      <pre className="avar-markdown__pre">
        <code className={className} {...props}>
          {children}
        </code>
      </pre>
    );
  },
};

export function MarkdownRenderer({
  content,
  remarkPlugins = [],
  className = "avar-markdown",
}: MarkdownRendererProps) {
  return (
    <Markdown
      className={className}
      remarkPlugins={[remarkGfm, ...remarkPlugins]}
      components={defaultComponents}
    >
      {content}
    </Markdown>
  );
}
