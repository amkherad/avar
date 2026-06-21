import { useEffect, useRef } from "react";
import { createPortal } from "react-dom";
import { FontAwesomeIcon } from "@/icons";
import type { IconDefinition } from "@fortawesome/fontawesome-svg-core";

export interface ContextMenuItem {
  id: string;
  label: string;
  onClick: () => void;
  icon?: IconDefinition;
  disabled?: boolean;
  danger?: boolean;
  checked?: boolean;
}

export interface ContextMenuProps {
  x: number;
  y: number;
  items: ContextMenuItem[];
  onClose: () => void;
}

export function ContextMenu({ x, y, items, onClose }: ContextMenuProps) {
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    function handlePointerDown(event: MouseEvent) {
      if (menuRef.current?.contains(event.target as Node)) {
        return;
      }
      onClose();
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        onClose();
      }
    }

    window.addEventListener("mousedown", handlePointerDown);
    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("mousedown", handlePointerDown);
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [onClose]);

  useEffect(() => {
    const menu = menuRef.current;
    if (!menu) {
      return;
    }

    const rect = menu.getBoundingClientRect();
    const maxX = window.innerWidth - rect.width - 8;
    const maxY = window.innerHeight - rect.height - 8;
    menu.style.left = `${Math.min(x, maxX)}px`;
    menu.style.top = `${Math.min(y, maxY)}px`;
  }, [x, y]);

  return createPortal(
    <div
      ref={menuRef}
      className="avar-context-menu"
      style={{ left: x, top: y }}
      role="menu"
      onContextMenu={(e) => e.preventDefault()}
    >
      <ul className="avar-context-menu__list">
        {items.map((item) => (
          <li key={item.id} role="none">
            <button
              type="button"
              role="menuitem"
              className={`avar-context-menu__item ${item.danger ? "avar-context-menu__item--danger" : ""}`}
              disabled={item.disabled}
              onClick={() => {
                if (!item.disabled) {
                  item.onClick();
                  onClose();
                }
              }}
            >
              {item.icon ? (
                <FontAwesomeIcon icon={item.icon} className="avar-context-menu__icon" />
              ) : item.checked ? (
                <span className="avar-context-menu__icon" aria-hidden="true">
                  ✓
                </span>
              ) : (
                <span className="avar-context-menu__icon avar-context-menu__icon--spacer" aria-hidden="true" />
              )}
              <span className="avar-context-menu__label">{item.label}</span>
            </button>
          </li>
        ))}
      </ul>
    </div>,
    document.body,
  );
}
