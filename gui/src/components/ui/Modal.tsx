import type { ReactNode } from "react";
import { FontAwesomeIcon } from "@/icons";
import { faXmark } from "@fortawesome/free-solid-svg-icons";
import { Button } from "./Button";

export interface ModalProps {
  open: boolean;
  title: string;
  onClose: () => void;
  children: ReactNode;
  footer?: ReactNode;
  wide?: boolean;
}

export function Modal({ open, title, onClose, children, footer, wide = false }: ModalProps) {
  if (!open) {
    return null;
  }

  return (
    <div className="avar-modal-backdrop" role="presentation" onClick={onClose}>
      <div
        className={`avar-modal ${wide ? "avar-modal--wide" : ""}`.trim()}
        role="dialog"
        aria-modal="true"
        aria-labelledby="avar-modal-title"
        onClick={(e) => e.stopPropagation()}
      >
        <header className="avar-modal__header">
          <h2 id="avar-modal-title">{title}</h2>
          <Button variant="ghost" size="sm" onClick={onClose} aria-label="Close">
            <FontAwesomeIcon icon={faXmark} />
          </Button>
        </header>
        <div className="avar-modal__body">{children}</div>
        {footer ? <footer className="avar-modal__footer">{footer}</footer> : null}
      </div>
    </div>
  );
}
