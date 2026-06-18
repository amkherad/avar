import type { ReactNode } from "react";

export interface CardProps {
  title?: string;
  actions?: ReactNode;
  children: ReactNode;
  className?: string;
}

export function Card({ title, actions, children, className = "" }: CardProps) {
  return (
    <section className={`avar-card ${className}`.trim()}>
      {title || actions ? (
        <header className="avar-card__header">
          {title ? <h3 className="avar-card__title">{title}</h3> : <span />}
          {actions ? <div className="avar-card__actions">{actions}</div> : null}
        </header>
      ) : null}
      <div className="avar-card__body">{children}</div>
    </section>
  );
}
