import type { CSSProperties, ReactNode } from "react";
import { FontAwesomeIcon } from "@/icons";
import { faXmark } from "@fortawesome/free-solid-svg-icons";
import { useDraggable } from "@/hooks/useDraggable";
import { Button } from "./Button";

export interface ModalProps {
  open: boolean;
  title: string;
  onClose: () => void;
  children: ReactNode;
  footer?: ReactNode;
  wide?: boolean;
  draggable?: boolean;
}

export function Modal({
  open,
  title,
  onClose,
  children,
  footer,
  wide = false,
  draggable = true,
}: ModalProps) {
  const { dragHandleProps, dialogStyle } = useDraggable({
    enabled: draggable,
    resetKey: open,
  });

  if (!open) {
    return null;
  }

  const modalStyle: CSSProperties = {
    ...dialogStyle,
  };

  return (
    <div className="avar-modal-backdrop" role="presentation" onClick={onClose}>
      <div
        className={`avar-modal ${wide ? "avar-modal--wide" : ""}`.trim()}
        role="dialog"
        aria-modal="true"
        aria-labelledby="avar-modal-title"
        style={modalStyle}
        onClick={(e) => e.stopPropagation()}
      >
        <header
          className={`avar-modal__header ${draggable ? "avar-modal__header--draggable" : ""}`.trim()}
          {...(draggable ? dragHandleProps : {})}
        >
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
